/* jsint.c
 * (c) 2002 Mikulas Patocka (Vrxni Ideolog), Petr 'Brain' Kulhavy
 * This file is a part of the Links program, relased under GPL.
 */

/*
 * Ve vsech upcallech plati, ze pokud dostanu ID nejakeho objektu, tak
 * javascript ma k tomu objektu pristupova prava. Jinymi slovy pristupova prava
 * se testuji v upcallech jen, aby se neco neproneslo vratnici ven. Dovnitr se
 * muze donaset vsechno, co si javascript donese, na to ma prava.
 *
 * Navic vsechny upcally dostanou pointer na f_data_c, kde bezi javascript,
 * takze se bude moci testovat, zda javascript nesaha na f_data_c, ke kteremu
 * nema pristupova prava.
 *
 *   Brain
 */

/* Uctovani pameti:
 * js_mem_alloc/js_mem_free se bude pouzivat na struktury fax_me_tender
 * dale se bude pouzivat take ve funkcich pro praci s cookies, protoze string
 * cookies v javascript_context se tez alokuje pomoci js_mem_alloc/js_mem_free.
 */

/*
   Retezce:

   vsechny retezce v ramci javascriptu jsou predavany v kodovani f_data->cp
   (tedy tak, jak prisly v dokumentu ze site)
 */


#include "links.h"

void jsint_execute_code(struct f_data_c *fd, unsigned char *code, int len, int write_pos, int onclick_submit, int onsubmit, struct links_event *ev)
{
}

void jsint_destroy(struct f_data_c *fd)
{
}

void jsint_scan_script_tags(struct f_data_c *fd)
{
}

int jsint_get_source(struct f_data_c *fd, unsigned char **start, unsigned char **end)
{
	return 0;
}
