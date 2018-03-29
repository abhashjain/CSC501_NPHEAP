//////////////////////////////////////////////////////////////////////
//                             North Carolina State University
//
//
//
//                             Copyright 2016
//
////////////////////////////////////////////////////////////////////////
//
// This program is free software; you can redistribute it and/or modify it
// under the terms and conditions of the GNU General Public License,
// version 2, as published by the Free Software Foundation.
//
// This program is distributed in the hope it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
// more details.
//
// You should have received a copy of the GNU General Public License along with
// this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
//
////////////////////////////////////////////////////////////////////////
//
//   Author:  Hung-Wei Tseng
//
//   Description:
//     Skeleton of NPHeap Pseudo Device
//
////////////////////////////////////////////////////////////////////////

#include "npheap.h"
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/poll.h>
#include <linux/mutex.h>
#include <linux/tty.h> /* For the tty declarations */
#include <linux/version.h> /* For LINUX_VERSION_CODE */
#include <linux/sched.h>

extern struct miscdevice npheap_dev;

/* Structure for Linked list, This is going to store the
	object ID and address for allocated memory */

struct Node{
	unsigned long key; //Key is Object ID given by process
	unsigned long size;
	struct Node* next;
	struct mutex lockmutex;
	void* area_pointer;
};

//Global variable for storing data in Linkedlist
struct Node* db_Head = NULL;
static void* kmalloc_ptr = NULL;
static int *kmalloc_area = NULL;

/*This function add the new node infront of Linked list*/
struct Node* addNode(struct Node** head,unsigned long key, void* new_data,unsigned long size)
{
    /* Allocate memory for node */
    struct Node* new_node = (struct Node*)kmalloc(sizeof(struct Node),GFP_KERNEL);
 	if (new_node == NULL){
		printk(KERN_ERR "\n New node can't be created- kmalloc failed!\n");
	}
	mutex_init(&(new_node->lockmutex));
	new_node->next = (*head);
    	// Copy contents of new_data to newly allocated memory.
	new_node->key = key;
	new_node->size = size;
	new_node->area_pointer = new_data;
    // Change head pointer as new node is added at the beginning
    (*head)    = new_node;
    return new_node;
}

/* This function will be used to delete the node given key*/
void deleteNode(struct Node* head,unsigned long key){
	// Store head node
    
	struct Node* temp = head;
	  struct Node *prev;

    // If head node itself holds the key to be deleted
    if (temp != NULL && temp->key == key)
    {
        head = temp->next;  // Changed head
		if (temp->area_pointer !=NULL)
		{
//			temp->size = 0;
			kfree(temp->area_pointer); // free the allocated area
		}
        temp->area_pointer = NULL;
		//kfree(temp);               // free old head
        return;
    }

    // Search for the key to be deleted, keep track of the
    // previous node as we need to change 'prev->next'
    while (temp != NULL && temp->key != key)
    {
        prev = temp;
        temp = temp->next;
    }

    // If key was not present in linked list, and we had reached at end of list
    if (temp == NULL) return;

    // Unlink the node from linked list
    prev->next = temp->next;

    //kfree(temp);  // Free memory
}


/*This function will be used to search a key in the list
	return link list  ptr to that node if found ,otherwise return NULL*/

struct Node* searchNode(struct Node* head,unsigned long key){
	struct Node* temp = head;
	//If Head is null then return NULL
	if (head == NULL)
		return NULL;
	while(temp != NULL){
		//printk("The current node's key: %lu. Search Node Key: %lu\n", temp->key, key);
		if (temp->key == key){
			return temp;
		}
		temp = temp->next;
	}
	return NULL;
}

/* Sample Function to print Link list*/
int printList(struct Node* head){
	int count = 0;
	struct Node* temp = head;
	while(temp!=NULL){
		printk("Element key: %ld and stored address: %lu \n",temp->key, temp->area_pointer != NULL ? *((unsigned long *)temp->area_pointer): 0L);
		temp = temp->next;
		count++;
	}
	return count;
}



int npheap_mmap(struct file *filp, struct vm_area_struct *vma)
{
	//to get the mmap page offset
	//unsigned long virt_addr;
	unsigned long offset = vma->vm_pgoff<<PAGE_SHIFT;
	unsigned long size = vma->vm_end - vma->vm_start;
	struct Node* temp = NULL;
	//passed offset
	unsigned long m_offset = offset / PAGE_SIZE;
	printk(KERN_ERR "vm_pgoff- %ld vma offset after shift is %ld and m_offset is %ld",vma->vm_pgoff, offset,m_offset);
	printk(KERN_ERR "Abhash: size from end to start is %ld",size);

	if (offset & ~PAGE_MASK){
		printk(KERN_ERR "offset not aligned: %ld\n", offset);
    	return -ENXIO;
	}
	if ((vma->vm_flags & VM_WRITE) && !(vma->vm_flags & VM_SHARED))
    {
         printk(KERN_ERR "writeable mappings must be shared, rejecting\n");
         return(-EINVAL);
    }
	//So this area can't we swap out from memory, we will lock it.
	vma->vm_flags |= VM_LOCKED;
//	printk (KERN_DEBUG "Abhash: Number of node presents %d\n",printList(db_Head));
	temp = searchNode(db_Head,m_offset);
	if (temp != NULL && temp->area_pointer != NULL){
		if(!mutex_is_locked(&temp->lockmutex))
		   mutex_lock_interruptible(&temp->lockmutex);
		printk(KERN_DEBUG "Node is already exist with offset %ld\n",m_offset);
		return remap_pfn_range(vma,vma->vm_start,virt_to_phys((void *)temp->area_pointer) >> PAGE_SHIFT,
					vma->vm_end - vma->vm_start, vma->vm_page_prot);
	}else {
		printk(KERN_DEBUG "Node is not present,adding a node with offset %ld\n",m_offset);
		kmalloc_ptr = kmalloc(size,GFP_KERNEL);
		memset(kmalloc_ptr,0,size);
		kmalloc_area=(int *)(((unsigned long)kmalloc_ptr + PAGE_SIZE -1) & PAGE_MASK);
		/*for (virt_addr=(unsigned long)kmalloc_area; virt_addr<(unsigned long)kmalloc_area+size;
	    	virt_addr+=PAGE_SIZE) {
	         //reserve all pages to make them remapable
	        mem_map_reserve(virt_to_page(virt_addr));
       }*/
		printk(KERN_DEBUG "Allocated memory from Kernal is %zu\n",ksize(kmalloc_ptr));

		if(remap_pfn_range(vma,vma->vm_start,virt_to_phys((void *)kmalloc_ptr) >> PAGE_SHIFT,
					vma->vm_end - vma->vm_start, vma->vm_page_prot)) {
			return -EAGAIN;
		}
		//after creating add the new entry to linked list
		if(temp == NULL)
		{
		  printk("Node not found, lock probably not called. Creating new node with offset: %ld\n",m_offset);
		  addNode(&db_Head,m_offset,kmalloc_ptr,size);
		}
		else {
			if(!mutex_is_locked(&temp->lockmutex))
	                   mutex_lock_interruptible(&temp->lockmutex);
			temp->area_pointer = kmalloc_ptr;
			temp->size = size;
		}

//		printk (KERN_DEBUG "Printing full List of allocations. Total Number of allocations: %d\n", printList(db_Head));
		}

    return 0;
}

int npheap_init(void)
{
    int ret;
    if ((ret = misc_register(&npheap_dev)))
        printk(KERN_ERR "Unable to register \"npheap\" misc device\n");
    else
        printk(KERN_ERR "\"npheap\" misc device installed\n");
	return ret;

}

void npheap_exit(void)
{
	struct Node *temp = db_Head;
	while(temp!=NULL){
		deleteNode(db_Head,temp->key);
		temp=temp->next;
	}
	db_Head = NULL;
	printk(KERN_DEBUG "Exiting from the module\n");
    misc_deregister(&npheap_dev);
}
