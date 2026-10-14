/* The 68000 runtime of the native port (no interpreter). See m68k_rt.c. */
#ifndef M68K_RT_H
#define M68K_RT_H

void m68k_rt_init(int (*int_ack_callback)(int));
void m68k_rt_reset(void);
void m68k_rt_set_irq(unsigned int int_level);
void m68k_rt_check_interrupts(void);
void m68k_rt_no_code(void);

#endif
