#include <acpi/aml/aml_parser.h>

static aml_node_t *def_alias(const char **src, size_t *len)
{
	const char *s;
	size_t l;
	aml_node_t *node;

	if(*len < 1 || **src != ALIAS_OP)
		return NULL;
	s = *src;
	l = *len;
	++(*src);
	--(*len);
	if(!(node = parse_node(src, len, 2, name_string, name_string)))
	{
		*src = s;
		*len = l;
		return NULL;
	}
	return node;
}

static aml_node_t *def_name(const char **src, size_t *len)
{
	// TODO
	(void) src;
	(void) len;
	return NULL;
}

static aml_node_t *def_scope(const char **src, size_t *len)
{
	// TODO
	(void) src;
	(void) len;
	return NULL;
}

aml_node_t *namespace_modifier_obj(const char **src, size_t *len)
{
	return parse_either(src, len, 3, def_alias, def_name, def_scope);
}
