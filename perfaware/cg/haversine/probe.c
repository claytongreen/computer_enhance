#include <stdint.h>
#include <stdio.h>

#include "os.h"

#include "os.c"

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;

  os_init();

  uint64_t pages = 4096;
  uint64_t page_size = os_page_size();

  printf("page count,touch count,fault count,extra faults,pml4_index,directory_ptr_index,directory_index,table_index,offset\n");
  for (uint64_t touch_count = 0; touch_count < pages; touch_count += 1) {

    uint8_t *memory = (uint8_t *)VirtualAlloc(0, (page_size * pages), MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
    if (memory) {
      uint64_t touch_size = touch_count * page_size;

      uint64_t page_faults_start = os_page_fault_count();
      for (uint64_t i = 0; i < touch_size; i += 1) {
        memory[i] = (uint8_t)i;
      }
      uint64_t page_faults_end = os_page_fault_count();
      uint64_t page_faults = page_faults_end - page_faults_start;

      void *ptr = (memory + (page_size*touch_count));
      uint64_t addr = (uint64_t)ptr;
      uint16_t pml4_index          = ((addr >> 39) & 0x1ff);
      uint16_t directory_ptr_index = ((addr >> 30) & 0x1ff);
      uint16_t directory_index     = ((addr >> 21) & 0x1ff);
      uint16_t table_index         = ((addr >> 12) & 0x1ff);
      uint32_t offset              = ((addr >>  0) & 0xfff);

      printf("%lld,%lld,%lld,%lld,%d,%d,%d,%d,%d\n", pages, touch_count, page_faults, (page_faults - touch_count), pml4_index, directory_ptr_index, directory_index, table_index, offset);

      VirtualFree(memory, 0, MEM_RELEASE);
    } else {
      printf(",,,\n");
    }
  }

  return 0;
}
