#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//age & name
struct entry {
  int age;
  char *name;
};

#define MAX_ENTRIES 1000
#define NAME_LEN_MAX 64

struct entry* directory[MAX_ENTRIES]; // entry ptrs array
int slots[MAX_ENTRIES]; // global ints array

struct entry *creation() {
    char name[64];
    struct entry *e;

    e = (struct entry *) malloc(sizeof(struct entry));

    printf("Name: ");
    fgets(name, NAME_LEN_MAX, stdin);
    name[strlen(name)-1] = 0; // no overflow
    e->name = malloc(strlen(name)); // name is dynamically allocated
                                    // chunk size is according to length
    strcpy(e->name, name);

    printf("Age: ");
    fgets(name,6,stdin);
    e->age = atoi(name); // accepts negative ints also

    return e;
}

void change(int e) {
    char name[64];

    printf("New name: ");
    fgets(name, strlen(directory[e]->name)+1, stdin);
    name[strlen(name)] = 0; // off by one error?
    
    strcpy(directory[e]->name, name);
    printf("New age: ");
    fgets(name, 6, stdin);
    directory[e]->age = atoi(name);
}

void delete(int i) {
    // as expected
    free(directory[i]->name);
    free(directory[i]);
}

int choice(int min, int max, char * menu) {
    int i;
    char buf[6]; // why 6?
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

void show(int i) {
    printf("[%d] %s, %d years old\n", i, directory[i]->name, directory[i]->age);
}

int main()
{
    int i,j; //locals
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
            for (j=0;(slots[j]>0 ) && (j<MAX_ENTRIES);j++);
            if (j < MAX_ENTRIES) {
                directory[j] = creation();
                slots[j] = 1; // mark as occupied
            }
            break;
        //delete entry
        case 2 :
            //expects entry index (0 .. MAX_ENTRIES-1)
            j = choice(0, MAX_ENTRIES-1, "Entry to delete: ");
            delete(j);
            slots[j] = 0; // mark as unused
            break;
        //Change entry
        case 3 : 
            j = choice(0, MAX_ENTRIES-1, "Entry to change: ");
            change(j);
            break;
        //Show all entries
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