#ifndef FAULT_HANDLERS_H
#define FAULT_HANDLERS_H

/* Fault handler prototypes */
void HardFault_Handler(void);
void MemManage_Handler(void);
void BusFault_Handler(void);
void UsageFault_Handler(void);
void DebugMon_Handler(void);

#endif /* FAULT_HANDLERS_H */