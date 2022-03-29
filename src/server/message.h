/*
 * File: message.h
 * Purpose: Message handling
 */

#ifndef INCLUDED_MESSAGE_H
#define INCLUDED_MESSAGE_H

#define msg_misc(P, A) msg_print_near((P), MSG_PY_MISC, (A))

extern void sound(struct player *p, int num);
extern void msg_broadcast(struct player *p, const char *msg, uint16_t type);
extern void msg_all(struct player *p, const char *msg, uint16_t type);
extern void msg(struct player *p, const char *fmt, ...);
extern void msg_print_complex_near(struct player *p, struct player *q, uint16_t type,
    const char *msg);
extern void msg_format_complex_near(struct player *p, uint16_t type, const char *fmt, ...);
extern void msg_format_complex_far(struct player *p, uint16_t type, const char *fmt,
    const char *sender, ...);
extern void msg_print_near(struct player *p, uint16_t type, const char *msg);
extern void msg_format_near(struct player *p, uint16_t type, const char *fmt, ...);
extern void msgt(struct player *p, unsigned int type, const char *fmt, ...);
extern void msg_print(struct player *p, const char *msg, uint16_t type);
extern void message_flush(struct player *p);
extern void msg_channel(int chan, const char *msg);

#endif /* INCLUDED_MESSAGE_H */
