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

struct Node{
        unsigned long key; //Key is Object ID given by process
		unsigned long size;
        //struct vm_area_struct *vma;
        //unsigned long vm_start;  //This will be the reference to the memory location
        struct Node* next;
		struct mutex lockmutex;
        void* area_pointer;
};

extern struct Node* db_Head;
struct Node* searchNode(struct Node* head,unsigned long key);
struct Node* addNode(struct Node** head,unsigned long key, void* new_data,unsigned long size);
int printList(struct Node* head);
void deleteNode(struct Node* head,unsigned long key);

// If exist, return the data.
long npheap_lock(struct npheap_cmd __user *user_cmd)
{
    //unsigned long addressOfHead;
    //addressOfHead = (db_Head == NULL ? 100: ((struct Node*)(db_Head))->key );
    struct Node* nodePointer = searchNode(db_Head,user_cmd->offset/PAGE_SIZE);
    if(db_Head != NULL && nodePointer != NULL)
    {
	printk("locking existing node for editing! Input key: %llu. The key of locked node: %lu \n",user_cmd->offset/PAGE_SIZE, nodePointer->key);
	if(nodePointer->area_pointer !=NULL)
		printk("Area pointer is null in locked node\n");
	else
		printk("Area pointer is not null\n");
	if(mutex_is_locked(&(nodePointer->lockmutex))){
		printk("Mutex is locked\n");
	}
	else
		printk("Mutex is not locked for this node, going to lock the node\n");
	mutex_lock_interruptible(&(nodePointer->lockmutex));
    }
    else
    {
	printk("Node not found to lock! Creating new node. The key of new node: %llu \n", user_cmd->offset/PAGE_SIZE);
	nodePointer = addNode(&db_Head, user_cmd->offset/PAGE_SIZE, NULL,0);
	printk("locking new node for editing! The key of locked node: %lu \n", nodePointer->key);
	if(mutex_is_locked(&(nodePointer->lockmutex))){
        printk("Mutex is locked, for new node created\n");
    }
    else{
        printk("Mutex is not locked for new added  node, going to lock the node\n");
		mutex_lock_interruptible(&(nodePointer->lockmutex));
    }
	}
    return 0;
}

long npheap_unlock(struct npheap_cmd __user *user_cmd)
{
    struct Node* nodePointer = searchNode(db_Head,user_cmd->offset/PAGE_SIZE);
    if(db_Head != NULL && nodePointer != NULL)
    {
        printk("Unlocking existing node for editing! Input key: %llu. The key of unlocked node: %lu \n",user_cmd->offset/PAGE_SIZE,nodePointer->key);
        if(mutex_is_locked(&(nodePointer->lockmutex))){
        	printk("Mutex is locked,before releaseing it\n");
    	}
    else
        printk("Mutex is not locked for released node, going to unlock the node\n");
	mutex_unlock(&(nodePointer->lockmutex));
    }
    else
    {
        printk("The node doesn't exist. Nothing to unlock \n");
    }
    return 0;
}

long npheap_getsize(struct npheap_cmd __user *user_cmd)
{
    struct Node* nodePointer = searchNode(db_Head,user_cmd->offset/PAGE_SIZE);
    printk("npheap_getsize- key: %llu",user_cmd->offset/PAGE_SIZE);
	//printk("npheap_getsize- key: %llu, total no nodes: %d",user_cmd->offset/PAGE_SIZE, printList(db_Head));
    if(nodePointer!=NULL) {
    	printk("npheap_getsize: key: %llu And Key found: %lu And returned Size: %ld", user_cmd->offset/PAGE_SIZE, nodePointer->key,nodePointer->size);
    	return nodePointer->size;
		//return ksize(nodePointer->area_pointer);
    }
    else{
		printk("npheap_getsize: Node pointer is null");
		return 0;
	}
}
long npheap_delete(struct npheap_cmd __user *user_cmd)
{
	struct Node* nodePointer = searchNode(db_Head,user_cmd->offset/PAGE_SIZE);
	printk("Delete :Requested key: %llu, total no nodes: %d",user_cmd->offset/PAGE_SIZE, printList(db_Head));
	if (nodePointer != NULL){
		deleteNode(db_Head, user_cmd->offset/PAGE_SIZE);
	}
    return 0;
}

long npheap_ioctl(struct file *filp, unsigned int cmd,
                                unsigned long arg)
{
    switch (cmd) {
    case NPHEAP_IOCTL_LOCK:
        return npheap_lock((void __user *) arg);
    case NPHEAP_IOCTL_UNLOCK:
        return npheap_unlock((void __user *) arg);
    case NPHEAP_IOCTL_GETSIZE:
        return npheap_getsize((void __user *) arg);
    case NPHEAP_IOCTL_DELETE:
        return npheap_delete((void __user *) arg);
    default:
        return -ENOTTY;
    }
}
