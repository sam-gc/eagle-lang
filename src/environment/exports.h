#ifndef EXPORTS_H
#define EXPORTS_H

typedef struct export_control export_control;

export_control *ec_alloc();
void ec_add_str(export_control *ec, const char *str);
void ec_add_wcard(export_control *ec, const char *str);
int ec_allow(export_control *ec, const char *str);
void ec_free(export_control *ec);

#endif
