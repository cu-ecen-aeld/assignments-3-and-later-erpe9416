/**
 * @file aesdchar.c
 * @brief Functions and data related to the AESD char driver implementation
 *
 * Based on the implementation of the "scull" device driver, found in
 * Linux Device Drivers example code.
 *
 * @author Dan Walkes
 * @date 2019-10-22
 * @copyright Copyright (c) 2019
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/fs.h> // file_operations
#include "aesdchar.h"
#include "aesd_ioctl.h"
int aesd_major =   0; // use dynamic major
int aesd_minor =   0;


MODULE_AUTHOR("Eric Percin"); 
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;



// llseek implementation for A9 supporing SEEK_SET, SEEK_CUR, and SEEK_END
loff_t aesd_llseek(struct file *filp, loff_t offset, int whence) {

    loff_t retpos; 
    struct aesd_dev *my_dev;
    size_t total_size = 0;

    my_dev = filp->private_data;
    
    PDEBUG("aesd_llseek: starting with offset=%lld, whence=%d", offset, whence);
    
    mutex_lock(&my_dev->buff_lock);
    
    for (int i = 0; i < AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED; i++) {
        if (my_dev->buff.entry[i].buffptr != NULL) {
            total_size += my_dev->buff.entry[i].size;
        }
    }
    
    mutex_unlock(&my_dev->buff_lock);   
    
    // Implementation strategy #2: add my own llseek function with logging and locking,
    // but use fixed_size_llseek for logic:
    retpos = fixed_size_llseek(filp, offset, whence, total_size);
    
    PDEBUG("aesd_llseek: returning position %lld", retpos);
    
    return retpos;

}

// ioctl implementation for A9
// First value = command to seek to in circular buffer
// Second value = zero referenced offset within this command to seek into
static long aesd_ioctl(struct file *filp, unsigned int cmd, unsigned long arg) 
{

    struct aesd_dev *my_dev = filp->private_data;
    struct aesd_seekto seekto;
    uint32_t entry_count = 0;
    uint32_t starting_index = 0;
    size_t retoffset = 0;
    int target_cmd;
    int current_index;
    
    PDEBUG("aesd_ioctl: cmd = %d, arg = %ld", cmd, arg);

    // Only handle AESDCHAR_IOCSEEKTO for now
    if (cmd != AESDCHAR_IOCSEEKTO) {
        PDEBUG("aesd_ioctl: command not supported");
        return -1;
    }

    // Use copy_from_user to obtain the value from userspace
    if (copy_from_user(&seekto, (struct aesd_seekto __user *)arg, sizeof(seekto))) {
        PDEBUG("aesd_ioctl: copy_from_user failed");
        return -1;
    }
        
    mutex_lock(&my_dev->buff_lock);
    
    // Count the number of entries and validate the command index
    for (int i = 0; i < AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED; i++) {
        if (my_dev->buff.entry[i].buffptr != NULL) {
            entry_count++;
        }
    }

    if (seekto.write_cmd >= entry_count) {
        mutex_unlock(&my_dev->buff_lock);
        PDEBUG("aesd_ioctl: invalid command index");
        return -1;
    }
    
    // Now index into cb from the oldest command, double checking validity
    starting_index = (my_dev->buff.out_offs + seekto.write_cmd) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
    
    if (seekto.write_cmd_offset >= my_dev->buff.entry[starting_index].size) {
        mutex_unlock(&my_dev->buff_lock);
        PDEBUG("aesd_ioctl: command index out of range");
        return -1;
    }
    
    current_index = my_dev->buff.out_offs; 
    target_cmd = (int)seekto.write_cmd;  

    // Sum the size of each command preceeding the target
    for (int i = 0; i < target_cmd; i++) {
        retoffset += my_dev->buff.entry[current_index].size;
        current_index = (current_index + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
    }
    
    // And add the offset from the target command itself to finish
    retoffset += seekto.write_cmd_offset;

    filp->f_pos = retoffset;    

    mutex_unlock(&my_dev->buff_lock);
    return 0;

} 


int aesd_open(struct inode *inode, struct file *filp)
{
    
    struct aesd_dev *my_dev;

    PDEBUG("open");
    
    // Handle open: set flip->private_data with our aesd_dev device struct
    // Use inode->i_cdev w/ container_of to locate within aesd_dev
    my_dev = container_of(inode->i_cdev, struct aesd_dev, cdev);
    filp->private_data = my_dev;
    
    return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
    PDEBUG("release");
    // Handle release:
    //struct aesd_dev *my_dev = filp->private_data;
    
    return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval = 0;
    struct aesd_dev *my_dev;
    size_t entry_offset_byte_rtn;
    struct aesd_buffer_entry *read_entry;
    size_t copy_bytes, total_bytes;
    PDEBUG("read %zu bytes with offset %lld",count,*f_pos);

    // Handle read
    
    my_dev = filp->private_data;
    
    
    mutex_lock(&my_dev->buff_lock);
    
    // Find which circular buffer entry contains the byte at f_pos
    read_entry =  aesd_circular_buffer_find_entry_offset_for_fpos(
                    &my_dev->buff,
                    (size_t)(*f_pos),
                    &entry_offset_byte_rtn);
    
    if (!read_entry) {
        // No data to return
        mutex_unlock(&my_dev->buff_lock);
        return 0;
    }
     
    // Determine how many bytes should be copied 
    total_bytes = read_entry->size - entry_offset_byte_rtn;
    if (count < total_bytes) {
        copy_bytes = count;
    }
    else {
        copy_bytes = total_bytes;
    }
     
    // Copy the bytes     
    if (copy_to_user(buf, read_entry->buffptr + entry_offset_byte_rtn, copy_bytes)) {
        // Copy failed...
        mutex_unlock(&my_dev->buff_lock);
        return -1;
    }     

    *f_pos += copy_bytes;
    
    mutex_unlock(&my_dev->buff_lock);
    
    retval = copy_bytes;
     
    return retval;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval = -ENOMEM;
    struct aesd_dev *my_dev;
    char *next_newline = NULL;
    struct aesd_buffer_entry command_entry;
    PDEBUG("write %zu bytes with offset %lld",count,*f_pos);
    
    // Handle write
    
    my_dev = filp->private_data;
    
    mutex_lock(&my_dev->buff_lock);
    
    // Allocate the partial write buffer, or expand existing by size 'count'
    if (my_dev->partial_write_buff == NULL) {
        my_dev->partial_write_buff = kmalloc(count, GFP_KERNEL);
        if (!my_dev->partial_write_buff) {
            // Allocation failed!
            mutex_unlock(&my_dev->buff_lock);
            return -1;
        }
        my_dev->bytes_stored = 0;
    }
    else {
        char *temp_buff;
        temp_buff = krealloc(my_dev->partial_write_buff, my_dev->bytes_stored + count, GFP_KERNEL);
        if (!temp_buff) {
            // Allocation failed!
            mutex_unlock(&my_dev->buff_lock);
            return -1;
        }
        my_dev->partial_write_buff = temp_buff;
    }
    
    // Copy from user space
    if (copy_from_user(my_dev->partial_write_buff + my_dev->bytes_stored, buf, count)) {
        // Copy failed!!
        mutex_unlock(&my_dev->buff_lock);
        return -1;
    }
    my_dev->bytes_stored += count;
    
    // Check for newlines in the partial write buff, which signify complete commands
    
    next_newline = NULL;
    next_newline = memchr(my_dev->partial_write_buff, '\n', my_dev->bytes_stored);
    while (next_newline) {
    
    
        // First, get the position and length of the complete command
        size_t newline_position = next_newline - my_dev->partial_write_buff;
        size_t command_length = newline_position + 1;
        size_t remaining;
        const char *overwritten_entry;
        
        // Store the command in a new buffer to insert into circular buffer
        char* command_buffer = kmalloc(command_length, GFP_KERNEL);
        if (!command_buffer) {
            // Allocation failed!
            mutex_unlock(&my_dev->buff_lock);
            return -1;
        }
        memcpy(command_buffer, my_dev->partial_write_buff, command_length);
    
        command_entry.buffptr = command_buffer;
        command_entry.size = command_length;
        
        // We are responsible for freeing any overwritten entries
        overwritten_entry = aesd_circular_buffer_add_entry(&my_dev->buff, &command_entry);
        if (overwritten_entry) {
            kfree(overwritten_entry);
        }
        
        remaining = my_dev->bytes_stored - command_length;
        memmove(my_dev->partial_write_buff, my_dev->partial_write_buff + command_length, remaining);
        my_dev->bytes_stored = remaining;
        
        // Continue the while loop if we have another newline
        next_newline = memchr(my_dev->partial_write_buff, '\n', my_dev->bytes_stored);
    }
    
    *f_pos += count;
    
    mutex_unlock(&my_dev->buff_lock);
    
    retval = count;
    
    return retval;
}

struct file_operations aesd_fops = {
    .owner =    THIS_MODULE,
    .read =     aesd_read,
    .write =    aesd_write,
    .open =     aesd_open,
    .release =  aesd_release,
    .llseek  = aesd_llseek,
    .unlocked_ioctl = aesd_ioctl,
};

static int aesd_setup_cdev(struct aesd_dev *dev)
{
    int err, devno = MKDEV(aesd_major, aesd_minor);

    cdev_init(&dev->cdev, &aesd_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &aesd_fops;
    err = cdev_add (&dev->cdev, devno, 1);
    if (err) {
        printk(KERN_ERR "Error %d adding aesd cdev", err);
    }
    return err;
}



int aesd_init_module(void)
{
    dev_t dev = 0;
    int result;
    result = alloc_chrdev_region(&dev, aesd_minor, 1,
            "aesdchar");
    aesd_major = MAJOR(dev);
    if (result < 0) {
        printk(KERN_WARNING "Can't get major %d\n", aesd_major);
        return result;
    }
    memset(&aesd_device,0,sizeof(struct aesd_dev));

    // Initialize the AESD specific portion of the device
    aesd_circular_buffer_init(&aesd_device.buff);
    aesd_device.partial_write_buff = NULL;
    aesd_device.bytes_stored = 0;
    mutex_init(&aesd_device.buff_lock);

    result = aesd_setup_cdev(&aesd_device);

    if( result ) {
        unregister_chrdev_region(dev, 1);
    }
    return result;

}

void aesd_cleanup_module(void)
{
    dev_t devno = MKDEV(aesd_major, aesd_minor);

    cdev_del(&aesd_device.cdev);

    // Cleanup AESD specific poritions here as necessary
    for (int i = 0; i < AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED; i++) {
        if (aesd_device.buff.entry[i].buffptr) {
            kfree(aesd_device.buff.entry[i].buffptr);
        }
    }
    mutex_destroy(&aesd_device.buff_lock);


    unregister_chrdev_region(devno, 1);
}



module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
