/* mailto.c
 * mailto:// processing
 * (c) 2002 Mikulas Patocka
 * This file is a part of the Links program, released under GPL.
 */

#include "links.h"


static void prog_func(struct terminal *term, struct list_head *list, unsigned char *param, unsigned char *name)
{
	unsigned char *prog, *cmd;
	prog = get_prog(list);
	if (!*prog) {
		msg_box(term, NULL, TEXT_(T_NO_PROGRAM), AL_CENTER, TEXT_(T_NO_PROGRAM_SPECIFIED_FOR), cast_uchar " ", name, cast_uchar ".", MSG_BOX_END, NULL, 1, TEXT_(T_CANCEL), msg_box_null, B_ENTER | B_ESC);
		return;
	}
	if ((cmd = subst_file(prog, param, 0))) {
		exec_on_terminal(term, cmd, cast_uchar "", 1);
		free(cmd);
	}
}

void mailto_func(struct session *ses, unsigned char *url)
{
	unsigned char *user, *host, *m;
	int f = 1;
	if (!(user = get_user_name(url))) goto fail;
	if (!(host = get_host_name(url))) goto fail1;
	f = 0;
	m = stracpy(user);
	add_to_strn(&m, cast_uchar "@");
	add_to_strn(&m, host);
	check_shell_security(&m);
	prog_func(ses->term, &mailto_prog, m, TEXT_(T_MAIL));
	free(m);
	free(host);
	fail1:
	free(user);
	fail:
	if (f) msg_box(ses->term, NULL, TEXT_(T_BAD_URL_SYNTAX), AL_CENTER, TEXT_(T_BAD_MAILTO_URL), MSG_BOX_END, NULL, 1, TEXT_(T_CANCEL), msg_box_null, B_ENTER | B_ESC);
}
