#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Assume x86-64 system (64bit)
// Due to "structure packing" sizeof(struct entry) is 16bytes (2*8Bytes)
// Thus, resulting in two implications: 
// 1. Its dynamic chunk size will always be 0x20 (16 <= 24)
// 2. First data qword overlaps age member  <=> fd metadata
//    Second data qword overlaps name pointer <=> bk/Tcache-key metadata
struct entry {
  int age;      // 4 Bytes
  char *name;   // 8 Bytes
};

#define MAX_ENTRIES 1000
#define NAME_LEN_MAX 64

// ------------ .bss ------------ //

// User array to hold pointers to allocated chunks user-data,
// Overwriting this structure (by creating a fake chunk overlapping it)
// could be useful
struct entry* directory[MAX_ENTRIES]; // entry ptrs array

// Used to mark which entries (indexes) are in "allocated"/occupied` state
int slots[MAX_ENTRIES]; // global ints array

// So far this is very typical to a heap exploitation excercise 
// ------------------------------//


struct entry *creation() {
    char name[NAME_LEN_MAX];
    struct entry *e;
    e = (struct entry *) malloc(sizeof(struct entry));

    printf("Name: ");

    // If user enters input with length >= NAME_LEN_MAX,
    // then the last char is '\0'
    // else - last non-null char is '\n' and needs to be replaced with '\0'
    fgets(name, NAME_LEN_MAX, stdin); // no overflow, will be null byte terminated instead

    name[strlen(name)-1] = 0; // In case of overflow attemp, char array is already null terminated,
                              // so it adds an extra null byte before the existing one.
                              // Otherwise - its just replacing '\n' with '\0'

    // name member is dynamically allocated
    // chunk size is according to string length
    // Thus: chunks sizes are 0x20 to 0x50 (Fastbins range)
    e->name = malloc(strlen(name)); 
                                    
    strcpy(e->name, name); // name is null terminated always, so this can't be used to leak info

    printf("Age: ");
    fgets(name,6,stdin); // no overflow
    e->age = atoi(name); // accepts negative ints also

    return e;
}


// Edit name & age members of an existing entry / fake chunk
// NOTE: No relloc/malloc/free is done in here
//       HOWEVER, AN OVERFLOW IS POSSIBLE HERE!
void change(int e) {
    char name[64];

    printf("New name: ");
    fgets(name, strlen(directory[e]->name)+1, stdin); // as expected. It won't let us overflow into another chunk
    name[strlen(name)] = 0; // TODO: Seems like this can't be exploited
    
    strcpy(directory[e]->name, name);
    printf("New age: ");
    fgets(name, 6, stdin);
    directory[e]->age = atoi(name); // allows us to edit 4 bytes exactly
}

// Free an entry
void delete(int i) {
    // Order of resource releasing is correct.
    // NOTE: only slots[i] gets marked as "free",
    //       But the pointer directory[i] isn't set to null,
    //       Which MIGHT gives us a Double-free + Use after free vulnerability.
    //       (Right now it's just a "bug")   
    free(directory[i]->name);
    free(directory[i]);
}


// Nothing intersting here, just gets a valid choice within a range from the user 
int choice(int min, int max, char *menu) {
    int i;
    char buf[6]; 
    i = -1;
    while( (i < min) || (i > max)) {
        printf("%s", menu);
        fgets(buf, 5, stdin);
        i = atoi(buf);//uses atoi
    }
    return i ;
}


int menu() {
    return(choice(1, 5, "Menu\n  1: New entry\n \
      2: Delete entry\n  3: Change entry\n  \
      4: Show all\n  5: Exit\n---> "));
}


// Might could be used for leaking libc/heap addresses 
void show(int i) {
    printf("[%d] %s, %d years old\n", i, directory[i]->name, directory[i]->age);
}

int main()
{
    int i,j; 
    setvbuf(stdin,  NULL, _IONBF, 0); //no buffering 
    setvbuf(stdout, NULL, _IONBF, 0); //no buffering 
    setvbuf(stderr, NULL, _IONBF, 0); //no buffering 

    printf("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
    printf("*Â Simple directory - beta version *\n");
    printf("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n\n");

    //initialize the global ints array
    for(i=0; i < MAX_ENTRIES; slots[i++] = 0);
    
    for(i=menu(); i!=5; i=menu() ) switch(i) { 
        //new entry
        case 1 :
            // Find the first unoccupied slot
            for (j=0;(slots[j]>0 ) && (j<MAX_ENTRIES);j++);
            
            // if slots array isn't full
            if (j < MAX_ENTRIES) {
                directory[j] = creation();
                slots[j] = 1; // mark as occupied
            }
            break;

        //delete entry
        case 2 :
            j = choice(0, MAX_ENTRIES-1, "Entry to delete: ");
            delete(j);
            slots[j] = 0; // mark as unused, pointer stays valid
            break;

        //Change entry
        case 3 : 
            j = choice(0, MAX_ENTRIES-1, "Entry to change: ");
            change(j);
            break;

        //Show all "marked" (occuppied) entries
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