## Challenge 3

#### Description
There is a problem with the following code, which can lead to arbirtary code execution.
The goal of this challenge is to spot the problem, and describe it (the problem itself and how to exploit it).

#### Rules
No debugging allowed.

#### Code
```c
#include <stdio.h>
#include <stdlib.h>

char username[512] = {1};
void (*_atexit)(int) =  exit;

void cp_username(char *name, const char *arg)
{
  while((*(name++) = *(arg++)));
  *name = 0; 
}

int main(int argc, char **argv)
{
  if(argc != 2)
    {
      printf("[-] Usage : %s <username>\n", argv[0]);
      exit(0);
    }
   
  cp_username(username, argv[1]);
  printf("[+] Running program with username : %s\n", username);
   
  _atexit(0);
  return 0;
}
```

-----------------
# Solution:

The program expects a single string argument (A username)
[ otherwise it won't do anything and will exit(0) ]

At the beginning, the global buffer ` char username[512] ` is only initialized at index zero (first char) to 1 by `= {1}; ` just before running ` main() ` but we will later see it doesn't realy matter for us (it might matter for the caller).  
The rest of this buffer might contain any "garbage" value (not neccessarily zeros, it's **compiler dependant**)   

Since it is global & initiallized (even partially) to a **non zero value**, the variable itself (will get converted to a label?) will refer to data block which will be stored in the **.data** segment (not in **.bss**).  
see
[https://en.wikipedia.org/wiki/Data_segment](https://en.wikipedia.org/wiki/Data_segment)  



Thus, when calling
` cp_username(username, argv[1]); `  
It will copy (override) the content of the ` name ` buffer which is ` username ` in our case     

![Memory layout](https://he-s3.s3.amazonaws.com/media/uploads/383f472.png)

## Moving to the fun part:

### [Buffer overflow]
where is ` void (*_atexit)(int) `  stored? Also in the **.data** segment (an **initialized global** ).  
by stored I mean where it is pointing to, because actually it would also get converted into a label)  

The memory layout depends on the order of the variables declaration. In our case (Luckily) the buffer is defined first so it would be like this:  
Mem Adrr | Data of | What's suppoused to be stored in it
---------|----------|---------
 X       | char username[512] | username (input) string
 X + 512 | _atexit | Addr of a function defintion

 Nothing prevents us from overrding the **_atexit** content inside **cp_username** by sending a string (**argv[1]**) of at least ` (512+sizeof(void*)) ` **bytes** 

Meaning this is a vulnurability (changing the execution flow to our **shellcode** when ` _atexit(0) ` gets called)    

 Q: How much can we override more than that without causing segmentation fault?  
 A: Need to check it, but I think till we reach the **.bss** segment

 Q: Why does it matter?  
 A: Because currently we know we have **for sure** 512 Bytes for storing our shellcode inside the **.data** segment (we will set **_atexit** to where the address of **cp_username**)  
 TODO: check if it is possible to use a -512 offset from **cp_username** within the shellcode or we must use constant values (and how to overcome ASLR in such case)

Theoretically we can also start the shellcode at `(char*)_atexit + 8 ` but I don't know how much space we will have there untill we get segfault


TODO: check if our shellcode needs to adhere to the signature ` void func(int) ` or not (I don't think it needs )