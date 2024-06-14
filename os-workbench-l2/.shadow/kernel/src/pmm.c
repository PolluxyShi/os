#include <common.h>
#include <spinlock.h>

#define MAGIC 1279870296
typedef struct __header_t{
    size_t size;
    int magic;
} header_t;
typedef struct __node_t{
    size_t size;
    struct __node_t* next;
} node_t;
// Global Variable
spin_lock_t alloc_lk;
node_t* head;
size_t UPPERBOUND(size_t sz){
    int num = 0; int i = 0;
    while(sz){ if(sz&1){num++;} sz >>= 1; i++;}
    if(num > 1) return 1 << i;
    else return 1 << (i-1);
}
void print_free_list(){
    node_t* pnode = head;
    printf("############################\n");
    while(pnode != NULL){
        printf("[%x, %x) %x\n",(uintptr_t)(pnode+1),(uintptr_t)(pnode+1)+pnode->size,pnode->size);
        pnode = pnode->next;
    }
    printf("############################\n\n");
}
bool can_alloc(node_t* fr_node, size_t size, size_t edge, void** ptr, node_t** next_node){
    void* end = (void*)(fr_node + 1) + fr_node->size;
    *ptr = (void*)ROUNDUP((void*)fr_node + sizeof(header_t) + sizeof(header_t**), edge);
    *next_node = (node_t*)((void*)(*ptr) + size);
    // printf("node_addr = 0x%x, node_size = 0x%x, node_end = 0x%x\n",fr_node,fr_node->size,end);////////
    // printf("ptr = 0x%x, next_node = 0x%x\n",*ptr,*next_node);///////
    if((uintptr_t)(*next_node) > (uintptr_t)end) return false;
    header_t* header = (header_t*)fr_node;
    memcpy((header_t**)(*ptr)-1,&header,sizeof(header_t*));
    if((uintptr_t)((*next_node)+1) < (uintptr_t)end){
        (*next_node)->next = fr_node->next;
        (*next_node)->size = (uintptr_t)end - (uintptr_t)((*next_node)+1);
        header->size = (uintptr_t)(*next_node) - (uintptr_t)(header + 1);
        header->magic = MAGIC;
    }else{
        *next_node = fr_node->next;
        header->size = fr_node->size + sizeof(node_t) - sizeof(header_t);
        header->magic = MAGIC;
    }
    return true;
}
static void *kalloc(size_t size) {
    size_t edge = UPPERBOUND(size);
    // printf("head = 0x%x, edge = %d, size = %d\n",head,edge,size);///////
    spin_lock(&alloc_lk);
    void* ret = NULL;
    node_t** p2nodep = &head;
    node_t* next_node = NULL;
    while (*p2nodep){
        if(can_alloc(*p2nodep,size,edge,&ret,&next_node)){
            (*p2nodep) = next_node;
            // printf("A [%x, %x) %x\n",ret,ret+size,size);
            // print_free_list();
            spin_unlock(&alloc_lk);
            return ret;
        }
        p2nodep = &((**p2nodep).next);
    }
    // printf("a\n");
    spin_unlock(&alloc_lk);
    return NULL;
}
static void kfree(void *ptr) {
    header_t* header = *((header_t**)ptr-1);
    // size_t free_size = (uintptr_t)header + sizeof(header_t) + header->size - (uintptr_t)ptr;
    if(header->magic != MAGIC){
        // printf("f\n");
        return;
    }
    spin_lock(&alloc_lk);
    node_t* node = (node_t*)header;
    node->size += sizeof(header_t) - sizeof(node_t);
    if((uintptr_t)node < (uintptr_t)head){
        if((uintptr_t)(node + 1) + node->size == (uintptr_t)head){
            node->next = head->next;
            node->size += head->size + sizeof(node_t);
            head = node;
        }else{
            node->next = head;
            head = node;
        }
    }else{
        node_t* p = head;
        while(p->next && !IN_RANGE(ptr,RANGE(p,p->next))) p = p->next;
        // printf("p = %x, node = %x, p->next = %x\n",p,node,p->next);
        // printf("before coalesce node & p->next: node->size = %x, p->next->size = %x\n",node->size,p->next?p->next->size:0);
        if((uintptr_t)(node + 1) + node->size == (uintptr_t)p->next){
            node->next = p->next->next;
            node->size += p->next->size + sizeof(node_t);
            p->next = node;
        }else{
            node->next = p->next;
            p->next = node;
        }
        // printf("after coalesce node & p->next: node->size = %x, p->next->size = %x\n",node->size,p->next?p->next->size:0);
        // printf("p = %x, p->next = %x\n",p,p->next);
        // printf("before coalesce p & node: p->size = %x, p->next->size = %x\n",p->size,p->next->size);
        if((uintptr_t)(p + 1) + p->size == (uintptr_t)p->next){
            p->size += p->next->size + sizeof(node_t);
            p->next = p->next->next;
        }
        // printf("after coalesce p & node: p->size = %x, p->next->size = %x\n\n",p->size,p->next->size);
    }
    // printf("F [%x, %x) %x\n",ptr,ptr+free_size,free_size);
    // print_free_list();
    // printf("Free %x bytes at addr = 0x%x!\n\n",free_size,ptr);
    spin_unlock(&alloc_lk);
    return;
}
static void pmm_init() {
    uintptr_t pmsize = ((uintptr_t)heap.end - (uintptr_t)heap.start);
    printf("Got %d MiB heap: [%p, %p)\n", pmsize >> 20, heap.start, heap.end);
    alloc_lk = SPIN_INIT();
    head = (node_t*)heap.start;
    *head = (node_t){(heap.end - heap.start) - sizeof(node_t), NULL};
}
MODULE_DEF(pmm) = {
    .init  = pmm_init,
    .alloc = kalloc,
    .free  = kfree,
};