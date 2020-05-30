/*
 *Copyright (C) 2004-2007 Qball Cow <qball@sarine.nl>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#ifndef WIN32
#include <sys/types.h>
#include <sys/stat.h>
#endif
#include "config1.h"

typedef enum _ConfigNodeType  {
	TYPE_CATEGORY,
	TYPE_ITEM,
	TYPE_ITEM_MULTIPLE
} ConfigNodeType;
/**
 * A config node
 * 24byte large
 */
typedef struct _config_node {
	struct _config_node *next;
	struct _config_node *prev;
	struct _config_node *parent;
	char *name;
	ConfigNodeType type;
	/* Save some extra memory by using a union
	 *  It is actually effective because we build a resonable large tree using this
	 */
	union{
		struct _config_node *children; 	/* TYPE_CATEGORY */
		char *value;			/* TYPE_ITEM* */
	};
} config_node;


typedef struct _config_obj
{
	char *url;
	config_node *root;
	int total_size;
} _config_obj;

static void __int_cfg_set_single_value_as_string(config_obj *, char *, char *, char *);
static void cfg_save(config_obj *);
static void __int_cfg_remove_node(config_obj *, config_node *);

static config_node *cfg_add_class(config_obj *, char *);
static config_node *cfg_new_node(void);
static void cfg_add_child(config_node *, config_node *);

static void cfg_open_parse_file(config_obj *cfgo, FILE *fp)
{
	char buffer[1024];
	int len = 0;
	int c;
	config_node *cur = NULL;
	config_node *multiple = NULL;
	while((c = fgetc(fp)) != EOF)
	{
		if(c == '[')
		{
			len =0;
			c = fgetc(fp);
			while(c != ']' && c != EOF && len < 1024){
				buffer[len] = c;
				len++;
				c = fgetc(fp);
			}
			buffer[len] = '\0';
			if(len > 0 && len < 256)
			{
				cur = cfg_add_class(cfgo, buffer);
			}
			/* seek end of line */
			while(c != EOF && c != '\n') c = fgetc(fp);
		}
		if(cur && c == '{')
		{
			len =0;                                    		
			c = fgetc(fp);
			while(c != '}' && c != EOF && len < 1024){
				buffer[len] = c;
				len++;
				c = fgetc(fp);
			}
			buffer[len] = '\0';
			if(len > 0 && len < 256)
			{
				config_node *child = cfg_new_node();
				child->type = TYPE_ITEM_MULTIPLE;
				child->name = strndup(buffer, len);
				child->children = NULL;
				cfg_add_child(cur, child);
				multiple = child;
				cfgo->total_size+= len+sizeof(config_node);

			}
			if(len==0)
			{
				multiple = NULL;
			}
			/* seek end of line */
			while(c != EOF && c != '\n') c = fgetc(fp);
		}

		/* next, ignore commants  and there must be a category*/
		else if(cur && (c  == '#' || c == '/' || c == '\n' || c == ';')){
			while(c != EOF && c != '\n') c = fgetc(fp);
		}
		else if(cur){
			config_node *new = NULL;
			len = 0;
			while(c != '=' && c != EOF){
				buffer[len] = c;
				len++;
				c = fgetc(fp);
			}
			if(len < 256 && len > 0)
			{
				int quote=0;

				/* write key name */
				new = cfg_new_node();
				new->type = TYPE_ITEM;
				new->name = strndup(buffer, len);	
				cfgo->total_size+= len+sizeof(config_node);
			/*	printf("node size: %i\n", sizeof(config_node));*/
				/* Get value */
				len = 0;
				/* skip spaces */
				while((c = fgetc(fp)) == ' ');
				/* we got a quoted string */
				if(c == '"')
				{
					quote= 1;
					c = fgetc(fp);
				}
				do{
					/* add escaped char */
					if(c == '\\'){
						c = fgetc(fp);
						if(c == 'n')
						{
							buffer[len] = '\n';
							len++;
						}
						else
						{
							buffer[len] = c;
							len++;
						}
					}
					/* We have a quoted string, and the closing quote comes */
					else if(c == '"' && quote) quote = -1;
					else{
						buffer[len] = c;
						len++;
					}
					c = fgetc(fp);
				}while((c != '\n' || quote) && c != EOF && quote >= 0 && len < 1024);
				new->value = strndup(buffer, len);
				cfgo->total_size+= len;
				if(multiple){
					cfg_add_child(multiple,new);
				}else{
					cfg_add_child(cur, new);
				}
			}
			/* seek end of line */
			while(c != EOF && c != '\n') c = fgetc(fp);			
		}
		else while(c != EOF && c != '\n') c = fgetc(fp);			
	}
}

config_obj *cfg_open(char *url)
{
	config_obj *cfgo = NULL;
	/* check if there is an url passed */
	if(url == NULL)
	{
		return NULL;
	}

	cfgo = malloc(sizeof(config_obj));
	/* check if malloc went ok */
	if(cfgo == NULL)
	{
		return NULL;
	}
	cfgo->url = strdup(url);
	cfgo->root = NULL;
	cfgo->total_size = sizeof(config_obj)+strlen(cfgo->url);

	/*if(g_file_test(cfgo->url, G_FILE_TEST_EXISTS))
	{
	*/	FILE *fp = fopen(cfgo->url, "r");
		if(fp)
		{
			cfg_open_parse_file(cfgo, fp);
			fclose(fp);
		}
	/*}*/
	return cfgo;
}


void cfg_close(config_obj *cfgo)
{
	if(cfgo == NULL)
	{
		return;
	}
	if(cfgo->url != NULL)
	{
		cfgo->total_size-=strlen(cfgo->url);
		cfg_free_string(cfgo->url);
	}
	while(cfgo->root)__int_cfg_remove_node(cfgo,cfgo->root);
	cfgo->total_size-= sizeof(config_obj);
	free(cfgo);
	cfgo = NULL;
}
static config_node *cfg_new_node()
{
	config_node *newnode = malloc(sizeof(config_node));
	newnode->type = TYPE_CATEGORY;
	newnode->name = NULL; 
	newnode->next = NULL;
	newnode->prev = NULL;                                 
	newnode->parent = NULL;
	newnode->value = NULL;
	return newnode;
}
static config_node *cfg_add_class(config_obj *cfg, char *class)
{
	config_node *newnode = cfg_new_node();
	newnode->type = TYPE_CATEGORY;
	newnode->name = strdup(class);
	newnode->value = NULL;
	newnode->children  = NULL;
	cfg->total_size += sizeof(config_node)+strlen(class);
	if(cfg->root == NULL)
	{
		cfg->root = newnode;		
	}
	else
	{
		config_node *temp = cfg->root;
		while(temp->next != NULL) temp = temp->next;
		temp->next = newnode;
		newnode->prev = temp;
	}
	return newnode;
}
void cfg_add_child(config_node *parent, config_node *child)
{
	if(parent == NULL || child == NULL) return;
	if(parent->type == TYPE_ITEM ) return;
	if(parent->children == NULL)
	{
		parent->children = child;
		child->parent = parent;
	}
	else
	{
		config_node *temp = parent->children;

		/* get last node */
		while(temp->next != NULL) temp = temp->next;
		temp->next = child;
		child->prev = temp;	
		child->parent = parent;
	}
}

static void cfg_save_category(config_obj *cfg, config_node *node, FILE *fp)
{
	config_node *temp = NULL;
	if(node == NULL)return;
	/* find the first */
	while(node->prev != NULL) node = node->prev;
	/* save some stuff */
	for(temp = node;temp != NULL; temp = temp->next){
		if(temp->type == TYPE_CATEGORY)
		{
			fprintf(fp, "\n[%s]\n\n",temp->name);
			cfg_save_category(cfg,temp->children,fp);
		}
		if(temp->type == TYPE_ITEM_MULTIPLE)
		{
			fprintf(fp, "\n{%s}\n",temp->name);
			cfg_save_category(cfg,temp->children,fp);
			fprintf(fp, "{}\n\n");
		}                                                		
		else if (temp->type == TYPE_ITEM)
		{
			int i= 0;
			fprintf(fp, "%s=\"", temp->name);
			for(i=0;i<strlen(temp->value);i++)
			{
				if(temp->value[i] == '"'){
					fputs("\\\"",fp);
				}
				else if(temp->value[i] == '\\'){
					fputs("\\\\",fp);
				}
				else if(temp->value[i] == '\n'){
					fputs("\\n",fp);
				}
				else{
					fputc(temp->value[i],fp);
				}
			}
			fputs("\"\n",fp);
		}
	}
}

static void cfg_save(config_obj *cfgo)
{
	if(cfgo == NULL)
	{
		return;
	}
	if(cfgo->root != NULL)
	{
		FILE *fp = fopen(cfgo->url, "w");
		if(!fp) return;
		cfg_save_category(cfgo,cfgo->root, fp);	
		fclose(fp);
#ifndef WIN32
		chmod(cfgo->url, 0600);
#endif

	}
	return;
}

static config_node *cfg_get_class(config_obj *cfg, char *class)
{
	config_node *node = cfg->root;
	if(node == NULL) return NULL;
	/* find the first */
	while(node->prev != NULL) node = node->prev;

	for(; node!= NULL; node = node->next)
	{
		if(node->type == TYPE_CATEGORY && !strcmp(node->name, class))
		{
			return node;
		}
	}

	return NULL;
}

static config_node *cfg_get_single_value(config_obj *cfg, char *class, char *key)
{
	/* take children */
	config_node *cur = NULL;
	cur = cfg_get_class(cfg, class);
	if(cur == NULL || cur->children == NULL)
	{
		return NULL;
	}
	cur = cur->children;
	for(;cur != NULL; cur = cur->next) {
		if(!strcmp(cur->name, key))
		{
			return cur;
		}
	}
	return NULL;                                     	
}

/*void cfg_free_string(char *string)
{
	if(string != NULL)
	{
		q_free(string);
	}
}
*/
static char * __int_cfg_get_single_value_as_string(config_obj *cfg, char *class, char *key)
{
	config_node *cur = cfg_get_single_value(cfg, class,key);
	if(cur != NULL)
	{
		if(cur->type == TYPE_ITEM)
		{
			return strdup((char *)cur->value);
		}
	}
	return NULL;
}

char * cfg_get_single_value_as_string(config_obj *cfg, char *class, char *key)
{
	char *retv = NULL;
	retv = __int_cfg_get_single_value_as_string(cfg,class,key);
	return retv;
}


char * cfg_get_single_value_as_string_with_default(config_obj *cfg, char *class, char *key , char *def)
{
	char *retv = NULL;
	retv = __int_cfg_get_single_value_as_string(cfg,class,key);
	if(retv == NULL)
	{
		__int_cfg_set_single_value_as_string(cfg,class,key,def);
		retv = __int_cfg_get_single_value_as_string(cfg,class,key);
	}
	return retv;
}

static int __int_cfg_get_single_value_as_int(config_obj *cfg, char *class, char *key)
{
	config_node *cur =NULL;
	int retv = CFG_INT_NOT_DEFINED;

	cur = cfg_get_single_value(cfg, class,key);
	if(cur != NULL)
	{
		if(cur->type == TYPE_ITEM)
		{
			retv = (int)strtoull(cur->value,NULL,0);	
		}
	}

	return retv; 
}
int cfg_get_single_value_as_int(config_obj *cfg, char *class, char *key)
{
	int retv = 0;
	retv = __int_cfg_get_single_value_as_int(cfg, class, key);
	return retv;
}

void cfg_set_single_value_as_int(config_obj *cfg, char *class, char *key, int value)
{
	char temp[64];
	snprintf(temp, 63, "%i", value);
	__int_cfg_set_single_value_as_string(cfg,class,key,temp);
	
}
static void __int_cfg_set_single_value_as_int(config_obj *cfg, char *class, char *key, int value)
{
	char temp[64];
	snprintf(temp, 63, "%i", value);
	__int_cfg_set_single_value_as_string(cfg,class,key,temp);
}
int cfg_get_single_value_as_int_with_default(config_obj *cfg, char *class, char *key, int def)
{
	int retv = CFG_INT_NOT_DEFINED;
	retv = __int_cfg_get_single_value_as_int(cfg,class,key);
	if(retv == CFG_INT_NOT_DEFINED)
	{
		__int_cfg_set_single_value_as_int(cfg,class,key,def);
		retv = __int_cfg_get_single_value_as_int(cfg,class,key);		
	}
	return retv;
}
/* float */
static void __int_cfg_set_single_value_as_float(config_obj *cfg, char *class, char *key, float value)
{
	char temp[64];
	snprintf(temp, 63, "%f", value);
	__int_cfg_set_single_value_as_string(cfg,class,key,temp);
}
static float __int_cfg_get_single_value_as_float(config_obj *cfg, char *class, char *key)
{
	config_node *cur =NULL;
	cur = cfg_get_single_value(cfg,class,key);
	float result = 0;
	if(cur == NULL)
	{
		return CFG_INT_NOT_DEFINED;
	}
	/* make it return an error */
	result = strtod(cur->value,NULL);
	return result;
}

float cfg_get_single_value_as_float(config_obj *cfg, char *class, char *key)
{
	float retv = 0;
	retv = __int_cfg_get_single_value_as_float(cfg,class,key);
	return retv;
}
float cfg_get_single_value_as_float_with_default(config_obj *cfg, char *class, char *key, float def)
 {
 	float retv = CFG_INT_NOT_DEFINED;
 	retv = __int_cfg_get_single_value_as_float(cfg,class,key);
 	if(retv == CFG_INT_NOT_DEFINED)
 	{
 		__int_cfg_set_single_value_as_float(cfg,class,key,def);
 		retv = __int_cfg_get_single_value_as_float(cfg,class,key);		
 	}
 	return retv;
 }
static void __int_cfg_remove_node(config_obj *cfg, config_node *node)
{
	if(node->type != TYPE_ITEM)
	{
		while(node->children)
		{
			__int_cfg_remove_node(cfg,node->children);
		}
	}
	/*  only child, and I have a parent */
	if(node->next == NULL && node->prev == NULL && node->parent)
	{
		/* remove from list */	
		if(node->parent->type != TYPE_ITEM)
		{
			node->parent->children = NULL;
		}
	}
	/* remove node from linked list */
	if(node->next != NULL)
	{
		if(node->parent && node->parent->children == node)
		{
			node->parent->children = node->next;
		}
		node->next->prev = node->prev;
	}
	if(node->prev != NULL)
	{
		if(node->parent && node->parent->children == node)
		{
			node->parent->children = node->prev;		
		}
		node->prev->next = node->next;
	}
	if(node == cfg->root)
	{
		if(node->next){
			cfg->root = node->next;
		}else if (node->prev){
			cfg->root = node->prev;
		}else{
			cfg->root = NULL;
		}
	}	
	cfg->total_size-= sizeof(config_node);
	if(node->name){
		cfg->total_size-=strlen(node->name);
		 cfg_free_string(node->name);
	}
	if(node->value) {
		cfg->total_size-=strlen(node->value);
		cfg_free_string(node->value);
	}
	free(node);
}
void cfg_del_single_value(config_obj *cfg, char *class, char *key)
{
	config_node *node = NULL;
   	node = cfg_get_single_value(cfg,class,key);
	if(node != NULL)
	{
		__int_cfg_remove_node(cfg,node);
		cfg_save(cfg);
	}
}
void cfg_remove_class(config_obj *cfg, char *class)
{
	config_node *node = NULL;
	if(cfg == NULL || class == NULL)
		return;

	node = cfg_get_class(cfg, class);
	if(node)
	{
		__int_cfg_remove_node(cfg, node);
	}
	cfg_save(cfg);
}

static void __int_cfg_set_single_value_as_string(config_obj *cfg, char *class, char *key, char *value)
{
	config_node *newnode = cfg_get_single_value(cfg,class,key);
	if(newnode == NULL)
	{	
		config_node *node = cfg_get_class(cfg, class);
		if(node == NULL)
		{
			node = cfg_add_class(cfg, class);	
			if(node == NULL) return;
		}
		newnode = cfg_new_node();
		newnode->name = strdup(key);
		cfg->total_size+=sizeof(config_node)+strlen(key);
		cfg_add_child(node,newnode);

	}	
	else if(strlen(newnode->value) == strlen(value) && !memcmp(newnode->value, value,strlen(newnode->value)))
	{
		/* Check if the content is the same, if it is, do nothing */
		return;
	}
	newnode->type = TYPE_ITEM;
	if(newnode->value){
		cfg->total_size-= strlen(newnode->value);
		cfg_free_string(newnode->value);
	}
	newnode->value = strdup(value);	
	cfg->total_size += strlen(value);
	cfg_save(cfg);
}

void cfg_set_single_value_as_string(config_obj *cfg, char *class, char *key, char *value)
{
	__int_cfg_set_single_value_as_string(cfg, class,key,value);
}
