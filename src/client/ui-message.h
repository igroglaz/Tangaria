/*
 * File: ui-message.h
 * Purpose: Message handling
 */

#ifndef INCLUDED_UI_MESSAGE_H
#define INCLUDED_UI_MESSAGE_H

/* Message iterator */
typedef struct
{
    char *str;
    uint16_t type;
    uint16_t count;
    uint8_t color;
} message_iter;

/* Functions */
extern void messages_init(void);
extern void messages_free(void);
extern uint16_t messages_num(void);
extern void message_add(const char *str, uint16_t type);
extern const char *message_str(uint16_t age);
extern uint16_t message_count(uint16_t age);
extern uint16_t message_type(uint16_t age);
extern uint8_t message_color(uint16_t age);
extern void message_color_define(uint16_t type, uint8_t color);
extern uint8_t message_type_color(uint16_t type);
extern int message_lookup_by_sound_name(const char *name);
extern const char *message_sound_name(int message);
extern const char *message_last(void);
extern void message_del(uint16_t age);
extern void message_first(message_iter *iter);
extern void message_next(message_iter *iter);
extern void sound(int type);
extern void bell(const char *reason);
extern void msg_flush(void);
extern void c_msg_print_aux(const char *msg, uint16_t type, bool memorize);
extern void c_msg_print(const char *msg);

#endif /* INCLUDED_UI_MESSAGE_H */
