#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "e1000_dev.h"
#include "net.h"

#define TX_RING_SIZE 16
static struct tx_desc tx_ring[TX_RING_SIZE] __attribute__((aligned(16)));
static struct mbuf *tx_mbufs[TX_RING_SIZE];

#define RX_RING_SIZE 16
static struct rx_desc rx_ring[RX_RING_SIZE] __attribute__((aligned(16)));
static struct mbuf *rx_mbufs[RX_RING_SIZE];

struct spinlock rx_ring_lock;
struct spinlock tx_ring_lock;

// remember where the e1000's registers live.
static volatile uint32 *regs;

struct spinlock e1000_lock;

int output = 0;

// called by pci_init().
// xregs is the memory address at which the
// e1000's registers are mapped.
void
e1000_init(uint32 *xregs)
{
  int i;

  initlock(&e1000_lock, "e1000");

  regs = xregs;

  // Reset the device
  regs[E1000_IMS] = 0; // disable interrupts
  regs[E1000_CTL] |= E1000_CTL_RST;
  regs[E1000_IMS] = 0; // redisable interrupts
  __sync_synchronize();

  // [E1000 14.5] Transmit initialization
  memset(tx_ring, 0, sizeof(tx_ring));
  for (i = 0; i < TX_RING_SIZE; i++) {
    tx_ring[i].status = E1000_TXD_STAT_DD;
    tx_mbufs[i] = 0;
  }
  regs[E1000_TDBAL] = (uint64) tx_ring;
  if(sizeof(tx_ring) % 128 != 0)
    panic("e1000");
  regs[E1000_TDLEN] = sizeof(tx_ring);
  regs[E1000_TDH] = regs[E1000_TDT] = 0;
  
  // [E1000 14.4] Receive initialization
  memset(rx_ring, 0, sizeof(rx_ring));
  for (i = 0; i < RX_RING_SIZE; i++) {
    rx_mbufs[i] = mbufalloc(0);
    if (!rx_mbufs[i])
      panic("e1000");
    rx_ring[i].addr = (uint64) rx_mbufs[i]->head;
  }
  regs[E1000_RDBAL] = (uint64) rx_ring;
  if(sizeof(rx_ring) % 128 != 0)
    panic("e1000");
  regs[E1000_RDH] = 0;
  regs[E1000_RDT] = RX_RING_SIZE - 1;
  regs[E1000_RDLEN] = sizeof(rx_ring);

  // filter by qemu's MAC address, 52:54:00:12:34:56
  regs[E1000_RA] = 0x12005452;
  regs[E1000_RA+1] = 0x5634 | (1<<31);
  // multicast table
  for (int i = 0; i < 4096/32; i++)
    regs[E1000_MTA + i] = 0;

  // transmitter control bits.
  regs[E1000_TCTL] = E1000_TCTL_EN |  // enable
    E1000_TCTL_PSP |                  // pad short packets
    (0x10 << E1000_TCTL_CT_SHIFT) |   // collision stuff
    (0x40 << E1000_TCTL_COLD_SHIFT);
  regs[E1000_TIPG] = 10 | (8<<10) | (6<<20); // inter-pkt gap

  // receiver control bits.
  regs[E1000_RCTL] = E1000_RCTL_EN | // enable receiver
    E1000_RCTL_BAM |                 // enable broadcast
    E1000_RCTL_SZ_2048 |             // 2048-byte rx buffers
    E1000_RCTL_SECRC;                // strip CRC
  
  // ask e1000 for receive interrupts.
  regs[E1000_RDTR] = 0; // interrupt after every received packet (no timer)
  regs[E1000_RADV] = 0; // interrupt after every packet (no timer)
  regs[E1000_IMS] = (1 << 7); // RXDW -- Receiver Descriptor Write Back

  initlock(&rx_ring_lock, "rx_ring_lock");
  initlock(&tx_ring_lock, "tx_ring_lock");

}

int
e1000_transmit(struct mbuf *m)
{
  //
  // Your code here.
  //
  // the mbuf contains an ethernet frame; program it into
  // the TX descriptor ring so that the e1000 sends it. Stash
  // a pointer so that it can be freed after sending.
  //


  // return 1;
  // 返回不为0的值，上层调用函数就能自己free这个mbuf了
  if(output){
    printf("enter e1000_transmit\n");
  }


  acquire(&tx_ring_lock);
  // printf("acquire e1000_lock\n");

  uint32 cur_tx_index = regs[E1000_TDT]; // 这里需要上锁么？
  

  // printf("release e1000_lock\n");
  
  // printf("cur_tx_index: %d\n", cur_tx_index);

  if((tx_ring[cur_tx_index].status & E1000_TXD_STAT_DD) != 1){
    printf("no available\n");
    return -1;  // 不空闲，返回错误，是应该返回-1么？
  }

  // if((tx_ring[cur_tx_index].status & E1000_TXD_STAT_DD) && tx_mbufs[cur_tx_index] != 0){
  //   printf("about to free\n"); // 这里是能运行到的
  // }

  // free the last mbuf if there was one
  if(tx_mbufs[cur_tx_index] != 0){
    mbuffree(tx_mbufs[cur_tx_index]);
    // printf("hhhh\n");
  }
  tx_mbufs[cur_tx_index] = m;
  
  

  // tx_ring[cur_tx_index].status = tx_ring[cur_tx_index].status | E1000_TXD_STAT_TU;

  tx_ring[cur_tx_index].addr = (uint64) m->head;
  tx_ring[cur_tx_index].length = m->len;
  tx_ring[cur_tx_index].status = tx_ring[cur_tx_index].status & (~E1000_TXD_STAT_DD);

  tx_ring[cur_tx_index].cmd = tx_ring[cur_tx_index].cmd | E1000_TXD_CMD_EOP | E1000_TXD_CMD_RS; // 待定

  // 其他的字段要不要设置呀
  // tx_ring[cur_tx_index].cso = 0;
  // tx_ring[cur_tx_index].css = 0;
  // tx_ring[cur_tx_index].special = 0;

  
  regs[E1000_TDT] = (regs[E1000_TDT] + 1) % TX_RING_SIZE; // 这里大概意味着发送
  
  
  // while(!(tx_ring[cur_tx_index].status & E1000_TXD_STAT_DD)){
  //   // return -1;
  //   // printf("while\n");  // 为什么这个printf有没有让测试通不通得过
  //   // 等待当前packet处理完
  // }

  
  release(&tx_ring_lock);

  if(output){
    printf("leave e1000_transmit\n");
  }

  // printf("transmit succeed\n");

  return 0;  // 成功
}

static void
e1000_recv(void)
{
  //
  // Your code here.
  //
  // Check for packets that have arrived from the e1000
  // Create and deliver an mbuf for each packet (using net_rx()).
  //
  // printf("enter e1000_recv\n");
  if(output){
    printf("enter e1000_recv\n");
  }

  // 上面要求创建一个mbuf，但是这里不是接受的么？从哪里获取到这个mbuf呢？
  acquire(&rx_ring_lock);
  // printf("acquire e1000_lock\n");
  
  uint32 cur_rx_index = (regs[E1000_RDT] + 1) % RX_RING_SIZE; // 这里需要上锁么？
  
  uint32 index = cur_rx_index;

  for(int i = 0; i < RX_RING_SIZE; i++){
    if ((rx_ring[index].status & E1000_RXD_STAT_DD)) {
      cur_rx_index = index;

      rx_mbufs[cur_rx_index]->len = rx_ring[cur_rx_index].length;

      // rx_ring[cur_rx_index].addr = (uint64)rx_mbufs[cur_rx_index]->head;
      // rx_ring[cur_rx_index].status &= (~E1000_RXD_STAT_DD);

      net_rx(rx_mbufs[cur_rx_index]);

      rx_mbufs[cur_rx_index] = mbufalloc(0);
      if (!rx_mbufs[cur_rx_index])
        panic("e1000");
      rx_ring[cur_rx_index].addr = (uint64)rx_mbufs[cur_rx_index]->head;
      rx_ring[cur_rx_index].status = 0;
      regs[E1000_RDT] = cur_rx_index;
    }
    index = (index + 1) % RX_RING_SIZE;
  }

  

  // printf("leave e1000_recv\n");

  release(&rx_ring_lock);
  // printf("release e1000_lock\n");
  // printf("e1000_recv succeed\n");
  if(output){
      printf("leave e1000_recv\n");
  }
  // printf("leave e1000_recv\n");

}

void
e1000_intr(void)
{
  // tell the e1000 we've seen this interrupt;
  // without this the e1000 won't raise any
  // further interrupts.
  regs[E1000_ICR] = 0xffffffff;

  e1000_recv();
}
