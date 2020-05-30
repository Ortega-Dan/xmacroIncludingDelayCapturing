#ifndef __CONFIG_1_H__
#define __CONFIG_1_H__
#define CFG_INT_NOT_DEFINED -65536

#include <stdio.h>

typedef struct _config_obj config_obj;

/**
 * open the config file
 */
config_obj *cfg_open(char *url);


/**
 * Close the object
 */
void cfg_close(config_obj *cfgo);
/* string */

/**
 * Get a single config value as a string.
 * It returns NULL when the value is not availible
 */
char * cfg_get_single_value_as_string(config_obj *cfg, char *class, char *key);
/**
 * set single value 
 */
void cfg_set_single_value_as_string(config_obj *cfg, char *class, char *key, char *value);
/**
 * get with default 
 */
char * cfg_get_single_value_as_string_with_default(config_obj *cfg, char *class, char *key , char *def);

int cfg_get_single_value_as_int(config_obj *cfg, char *class, char *key);
void cfg_set_single_value_as_int(config_obj *cfg, char *class, char *key, int value);
int cfg_get_single_value_as_int_with_default(config_obj *cfg, char *class, char *key, int def);

/* float */
float cfg_get_single_value_as_float(config_obj *cfg, char *class, char *key);
void cfg_set_single_value_as_float(config_obj *cfg, char *class, char *key, float value);
float cfg_get_single_value_as_float_with_default(config_obj *cfg, char *class, char *key, float def);

/* del */
void cfg_del_single_value(config_obj *cfg, char *class, char *key);

#define cfg_free_string(a)  free(a);a=NULL;
#endif
