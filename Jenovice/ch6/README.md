## Challenge 6

#### Description
The following code belongs to a remote service.
The goal of this challenge is to find the problem (if there is more than 1, then find all the problems) and describe it fully, including a description of how to exploit it.

[*] Bonus points for compiling it (fully protected - PIE, ASLR, W^X) and writing a fully working exploit (locally).

#### Rules
None.

#### Code
```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct entry {
  int age;
  char *name;
};

#define MAX_ENTRIES 1000
#define NAME_LEN_MAX 64

struct entry *directory[MAX_ENTRIES];
int slots[MAX_ENTRIES];

struct entry *creation() {
    char name[64];
    struct entry *e;

    e = (struct entry *) malloc(sizeof(struct entry));

    printf("Name: ");
    fgets(name, NAME_LEN_MAX, stdin);
    name[strlen(name)-1] = 0;
    e->name = malloc(strlen(name));
    strcpy(e->name, name);

    printf("Age: ");
    fgets(name,6,stdin);
    e->age = atoi(name);

    return e;
}

void change(int e) {
    char name[64];

    printf("New name: ");
    fgets(name, strlen(directory[e]->name)+1, stdin);

    name[strlen(name)] = 0;
    strcpy(directory[e]->name, name);
    printf("New age: ");
    fgets(name, 6, stdin);
    directory[e]->age = atoi(name);
}

void delete(int i) {
    free(directory[i]->name);
    free(directory[i]);
}

int choice(int min, int max, char * chaine) {
    int i;
    char buf[6];
    i = -1;
    while( (i < min) || (i > max)) {
        printf("%s", chaine);
        fgets(buf, 5, stdin);
        i = atoi(buf);
    }
    return i ;
}

int menu() {
    return(choice(1, 5, "Menu\n  1: New entry\n  2: Delete entry\n  3: Change entry\n  4: Show all\n  5: Exit\n---> "));
}

void show(int i) {
    printf("[%d] %s, %d years old\n", i, directory[i]->name, directory[i]->age);
}

int main()
{
    int i,j;
    setvbuf(stdin,  NULL, _IONBF, 0);
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    printf("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
    printf("*Â Simple directory - beta version *\n");
    printf("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n\n");
    for(i=0; i < MAX_ENTRIES; slots[i++] = 0);
    for(i=menu();i!=5;i=menu()) switch(i) { 
        case 1 :
            for (j=0;(slots[j]>0 ) && (j<MAX_ENTRIES);j++);
            if (j < MAX_ENTRIES) {
                directory[j] = creation();
                slots[j] = 1;
            }
            break;
        case 2 :
            j = choice(0, MAX_ENTRIES-1, "Entry to delete: ");
            delete(j);
            slots[j] = 0;
            break;
        case 3 : 
            j = choice(0, MAX_ENTRIES-1, "Entry to change: ");
            change(j);
            break;
        case 4 :
            printf("Entries:\n");
            for (j=0;j<MAX_ENTRIES;j++)
                if (slots[j])
                    show(j);
            printf("\n");
            break;
        default : break;
    }
}
```

## Solution (X64):
----------------
The goal here is to open a shell (even better as root)

Depending on glibc version and whether tcache is on/off:
old versions - fastbin dup attack (double free)
v2.26/v2.27: - tcache dup attack

v2.28: - tcache dump 
v2.31: - some extra mitigations on size fields etc.

  Override the __free_hook / __malloc_hook / atoi with system() address and create a fake (dup) chunk to point to it. send it "/bin/sh'/0'" as an argument or use one_gadget to solve for a specific glibc build.

----------------------------------------------------------------
### Static analysis:

The first thing to note is the structs:

```c
struct entry {
  int age;
  char *name;
};

#define MAX_ENTRIES 1000
#define NAME_LEN_MAX 64

struct entry *directory[MAX_ENTRIES];
int slots[MAX_ENTRIES];
```

Each time we'll create an entry, two memory chunkgs will be allocated on the heap: one for the entry, and one for its name
[ age(int) | name(char*)--]--> [actual name str(64B Max)]

On a **X64** platform:
` sizeof(stuct entry) ` = 4/8B +8B => 0x20 chunk

deletion (free) is done in reverse order as it should be.

If we will choose to allocate with **name** of len 56 chars, then the chunk allocated for it will be **0x40** (56+8=64=0x40, 8B for the size field. not counting the other first 8B for the prev field).
----------------
### Theory: 
----------------  
Fastbin & Tcache Unlink
The fastbins and tcache use singly linked, LIFO lists. Unlinking chunks from these lists simply involves copying the victim chunk’s fd into the head of the list. For more information on fastbins see the Arenas section and for more information on the tcache see the Tcache section.


Fastbins
The malloc source describes fastbins as “special bins that hold returned chunks without consolidating their spaces”. They are a collection of singly linked, non-circular lists that each hold free chunks of a specific size. There are 10 fastbins per arena*, each responsible for holding free chunks with sizes 0x20 through 0xb0. For example, a 0x20 fastbin only holds free chunks with size 0x20, and a 0x30 fastbin only holds free chunks with size 0x30, etc. Although only 7 of these fastbins are available under default conditions, the mallopt() function can be used to change this number by modifying the global_max_fast variable.
The head of each fastbin resides in its arena, although the links between subsequent chunks in that bin are stored inline. The first quadword of a chunk’s user data is repurposed as a forward pointer (fd) when it is linked into a fastbin. A null fd indicates the last chunk in a fastbin.

Fastbins are last-in, first-out (LIFO) structures, freeing a chunk into a fastbin links it into the head of that fastbin. Likewise, requesting chunks of a size that match a non-empty fastbin will result in allocating the chunk at the head of that fastbin.
Free chunks are linked directly into their corresponding fastbin if their corresponding tcachebin is full. Fastbin searches are conducted after a tcache search and before any other bins are searched, when the request size falls into fastbin range.
*The 0xb0 bin is created in error, this is because of the disparity between how the MAX_FAST_SIZE constant is treated as a request size versus how the global_max_fast variable is treated as a chunk size.

Tcache
In GLIBC versions >= **2.26** each thread is allocated its own structure called a tcache, or thread cache.  
A tcache behaves like an arena, but unlike normal arenas tcaches aren’t shared between threads.  
They are created by allocating space on a heap belonging to their thread’s arena and are freed when the thread exits.   
A tcache’s purpose is to relieve thread contention for malloc’s resources by giving each thread its own collection of chunks that aren’t shared with other threads using the same arena.  


A tcache takes the form of a tcache_perthread_struct, shown below in Figure 11, which holds the **head(s) of 64 tcachebins preceded by an array of counters** which record the number of free chunks in each tcachebin.  

Note that here “counts” is represented as an array of words, which is only the case in GLIBC versions >= **2.30**, prior to this it was an array of chars.  
Under default conditions a tcache holds chunks with sizes **0x20 – 0x410** inclusive. These tcachebins behave similarly to **fastbins**, with each acting as the head of a singly linked, non-circular list of free chunks of a specific size.  
The first entry in the counts array keeps track of the number of free chunks linked into the 0x20 tcachebin, the second entry tracks the 0x30 tcachebin etc.  
Under default conditions there is a limit imposed on the number of free chunks a tcachebin can hold, this number is held in the **malloc_par** struct under the **tcache_count** member.  
When a tcachebin’s count reaches this **limit**, free chunks of that bin’s size are instead treated as they would be **without a tcache** present. For example, if the 0x20 tcachebin is full (it holds 7 free chunks) the next 0x20-sized chunk to be freed would be linked into the 0x20 fastbin.  
Malloc uses a tcache’s counts array to determine whether a bin is full.

Allocations from a thread’s tcache take priority over its arena, this operation is performed from the **__libc_malloc()** function and does not enter **_int_malloc()**.  
Freed chunks in tcache size range are linked into a thread’s tcache unless the target tcachebin is full, in which case the thread’s **arena** is used.  
Note that tcache entries use pointers to **user data** rather than chunk metadata.  

### tcache Dup
Overview
Leverage a **double-free** bug to coerce malloc into returning the same chunk twice, without freeing it in between.  
This technique is typically capitalised upon by corrupting **tcache metadata** to **link a fake chunk** into a tcachebin. This fake chunk can be allocated, then program functionality could be used to read from or write to an arbitrary memory location.  

Detail:   
The **Tcache Dup** technique operates in a similar manner to the **Fastbin Dup**, the primary difference being that in GLIBC versions < **2.29** there is no **tcache double-free mitigation**.  
The Tcache Dup is a very powerful primitive because there is **no chunk size integrity check** on allocations from a tcachebin, making it very easy to overlap a fake tcache chunk with any memory address.



Further use
In GLIBC version **2.29** a **tcache double-free check** was introduced:  
A chunk is linked into a tcachebin, the **address of that thread’s tcache** is written into the slot usually **reserved** for a **free chunk’s** **bk pointer**, which is relabelled as a **“key” field**. 

When chunks are freed, their **key** field is **checked** and **if it matches** the **address** of the **tcache** then the appropriate tcachebin is **searched** for the **freed chunk**.  
**If** the chunk is found to be **already in the tcache** then **abort**() is called.  

This check can be **bypassed** by **filling the target tcachebin** to free a (same sized) victim chunk into the **same sized fastbin** afterwards, **emptying** the tcachebin **then** freeing the victim chunk a **2nd time**.  
Next, the victim chunk is **allocated from the tcachebin** at which point a designer can **tamper** with its **fastbin fd pointer**.  
When the victim chunk is allocated from its fastbin, the **remaining chunks** in the **same fastbin** are **dumped** into the **tcache**, including the **fake chunk**, tcache dumping **does not include a double-free check**.  
Note that the fake chunk’s fd **must be null** for this to succeed (TODO: check why).  

Since the tcache itself resides on the heap, it can be subject to corruption after a heap leak.  

----------------------------------------------------------------

Tcache Dumping
In versions of GLIBC compiled with tcache support, chunks in tcache size range are **dumped** into a tcache when a thread is allocated a chunk from its arena.  
When a chunk is allocated from the **fastbins** or **smallbins**, malloc dumps any **remaining free chunks** in that bin into their corresponding tcachebin **until it is full**, as shown in Figure 12 below.

When an **unsortedbin** scan occurs, malloc dumps any **exact-fitting** chunks it finds into their corresponding tcachebin. If the target tcachebin is full and malloc finds another exact-fitting chunk in the unsortedbin, that chunk is allocated.  If the unsortedbin scan is completed and one or more chunks were dumped into a tcachebin, a chunk is allocated from that tcachebin.
<br></br>
# Appendix:  
[tcache dump CTF challange - bctf2018 atum](https://github.com/Hi-Im-darkness/CTF/tree/master/bctf2018/pwn/atum)
[tcache dump - (v2.29-v2.31)](https://www.fatalerrors.org/a/tcache-stash-fastbin-double-free.html)  


[glibc v2.29 TCache Double Free mitigation bypass technique when double free and an intermediate memory corruption is possible](https://drive.google.com/file/d/1g2qIENh2JBWmYgmfTJMJUier8w0XAGDt/view)  



[tcache related challange 1](https://faraz.faith/2019-10-12-picoctf-2019-heap-challs/)
[tcache related challange 2](https://payatu.com/blog/Gaurav-Nayak/introduction-of-tcache-bins-in-heap-management)  

[Remote Code Execution via Tcache Poisoning - SANS SEC 760 "Baby Heap" CTF](https://www.youtube.com/watch?v=43ewpRBIRgA)