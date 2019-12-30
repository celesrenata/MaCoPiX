//  MaCoPiX = Mascot Construnctive Pilot for X
//                                (ActX / Gtk+ Evolution)
//
//
//     mail.c
//     Biff functions for MaCoPiX (POP3 /APOP and local spool)
//    
//                            Copyright 2002-2008  K.Chimari
//                                     http://rosegray.sakura.ne.jp/
//
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 2 of the License, or
//  (at your option) any later version.
// 
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
// 
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA.
//


#include "main.h"

#ifdef USE_BIFF

#ifdef USE_WIN32
#include <windows.h>
#endif

#include <dirent.h>

#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif

#include <signal.h>
#include "codeconv.h"

#define BUFFSIZE 200
#define HEADER_MAX 256
#define MIME_BUFFSIZE 1024

void biff_init();

gint ResetMailChecker();
gboolean MailChecker();

void mail_check();
void mail_check_mbox();
void mail_check_qmail();
void mail_check_pop3();
int get_pop3();
gpointer thread_get_pop3();
void thread_cancel_pop3();
gchar * fs_get_mbox();
gchar * fs_get_procmail();
gchar * fs_get_qmail();
void fs_get_pop3();
void pop3_error();

gchar * my_str_add();

void make_fs_max();
static void close_biff();
static void mailer_start();

void strip_last_ret();

// global argument
time_t former_newest;


void biff_init(typMascot *mascot)
{
  pop_debug_print("\nBiff init\n");

  mascot->mail.size=0;
  mascot->mail.last_check=0;
  mascot->mail.count=0;
  mascot->mail.fetched_count = 0;    // POP3 fetched fs this access
  mascot->mail.displayed_count = 0;  // POP3 displayed fs
  mascot->mail.spam_count = 0;       // SPAM count
  mascot->mail.status = NO_MAIL;
  mascot->mail.pop3_fs_status = POP3_OK_NORMAL;
  if(mascot->mail.last_f) g_free(mascot->mail.last_f);
  mascot->mail.last_f= NULL;
  if(mascot->mail.last_s) g_free(mascot->mail.last_s);
  mascot->mail.last_s=NULL;
  if(mascot->mail.pop_froms) g_free(mascot->mail.pop_froms);
  mascot->mail.pop_froms=NULL;
  gtk_widget_set_tooltip_text(mascot->biff_pix,
			      _("Initializing Biff..."));

  if(mascot->mail.tooltips_fl){
    gtk_widget_set_tooltip_text(mascot->biff_pix,NULL);
  }
  else{
    gtk_widget_set_tooltip_text(mascot->biff_pix,NULL);
  }
  mascot->mail.pop_readed=FALSE;

  if((mascot->mail.type==MAIL_POP3)||(mascot->mail.type==MAIL_APOP)){
    if(mascot->mail.pop_pass==NULL){
      if((mascot->mail.pop_server!=NULL)&&(mascot->mail.pop_id!=NULL)){
	create_pop_pass_dialog(mascot);
	// Get Pop Pass from GUI
      }
    }
  }

  if(mascot->mail.ssl_cert_skip)
    mascot->mail.ssl_cert_res=SSL_CERT_ACCEPT;
  else mascot->mail.ssl_cert_res=SSL_CERT_NONE;
  if(mascot->mail.ssl_sub) g_free(mascot->mail.ssl_sub);
  mascot->mail.ssl_sub=NULL;
  if(mascot->mail.ssl_iss) g_free(mascot->mail.ssl_iss);
  mascot->mail.ssl_iss=NULL;
  mascot->mail.ssl_verify=0;
}    


gboolean MonitorBiff (gpointer gdata){
  typMascot *mascot=(typMascot *)gdata;


  if(mascot->mail.flag){
    if(!mascot->mail.running){
      if(mascot->mail.init){
	biff_init(mascot);
	mascot->mail.init=FALSE;
      }
      mascot->mail.proc_id
	=(gint)g_timeout_add(100,
			     (GSourceFunc)MailChecker,
			     (gpointer)mascot);
      mascot->mail.running=TRUE;
      pop_debug_print("proc_id=%d\n",mascot->mail.proc_id);
    }
    else if (mascot->mail.proc_id<0){  // 2nd time
      mascot->mail.proc_id
	=(gint)g_timeout_add((guint32)(mascot->mail.interval*1000),
			     (GSourceFunc)MailChecker,
			     (gpointer)mascot);
      
      pop_debug_print("proc_id=%d\n",mascot->mail.proc_id);
    }
  }

  return(TRUE);
}


gint SetMailChecker(typMascot *mascot){
  thread_cancel_pop3(mascot);

  if(mascot->mail.proc_id>0){
    g_source_remove((guint)mascot->mail.proc_id);
    mascot->mail.proc_id=-1;
    mascot->mail.running=FALSE;
  }
  map_biff(mascot, FALSE);

  mascot->mail.init=TRUE;
  
  return(0);
}



gint ResetMailChecker(typMascot *mascot){
  thread_cancel_pop3(mascot);
  mascot->mail.running=FALSE;
 
  if(mascot->mail.proc_id>0){
    g_source_remove((guint)mascot->mail.proc_id);
    mascot->mail.proc_id=-1;
    mascot->mail.running=FALSE;
  }
  map_biff(mascot, FALSE);

  mascot->mail.init=FALSE;

  return (0);
}


gboolean MailChecker(gpointer gdata){
  typMascot *mascot;

  mascot=(typMascot *)gdata;

  if(mascot->flag_menu) return(TRUE);

  gdkut_flush(mascot->win_main);

  ext_play(mascot,mascot->mail.polling);  
  mail_check(mascot);

  gdkut_flush(mascot->win_main);

  if((mascot->mail.type!=MAIL_POP3)&&(mascot->mail.type!=MAIL_APOP))
    display_biff_balloon(mascot);

  gdkut_flush(mascot->win_main);
  mascot->mail.proc_id=-1;
  return(FALSE);
}


void make_biff_pix(typMascot *mascot){
  GtkWidget *ebox;
  
  mascot->biff_pix = gtk_window_new(GTK_WINDOW_POPUP);
  gtk_window_set_accept_focus(GTK_WINDOW(mascot->biff_pix),FALSE);
  gtk_widget_set_app_paintable(mascot->biff_pix, TRUE);

  ebox=gtk_event_box_new();
  gtk_container_add (GTK_CONTAINER (mascot->biff_pix), ebox);
  
  mascot->dw_biff = gtk_drawing_area_new();
  gtk_widget_set_size_request (mascot->dw_biff, 1, 1);
  gtk_container_add(GTK_CONTAINER(ebox), mascot->dw_biff);
  gtk_widget_set_app_paintable(mascot->dw_biff, TRUE);

  gtk_widget_set_events(ebox, 
			GDK_FOCUS_CHANGE_MASK | 
			GDK_BUTTON_MOTION_MASK | 
			GDK_BUTTON_RELEASE_MASK | 
			GDK_BUTTON_PRESS_MASK | 
			GDK_EXPOSURE_MASK);
  
  gtk_widget_set_events(mascot->dw_biff, 
			GDK_STRUCTURE_MASK | GDK_EXPOSURE_MASK);
  
  gtk_widget_realize(mascot->biff_pix);
  gtk_widget_realize(mascot->dw_biff);
  gtk_window_set_resizable(GTK_WINDOW(mascot->biff_pix),TRUE);
  gdk_window_set_decorations(gtk_widget_get_window(mascot->biff_pix), 0);
#ifndef USE_WIN32
  /* gdk_window_set_override_redirect is not implemented (for warning) */
  gdk_window_set_override_redirect(gtk_widget_get_window(mascot->biff_pix),TRUE);
#endif

  my_signal_connect(mascot->dw_biff, "configure_event",
  		    dw_configure_biff_pix, (gpointer)mascot);
#ifdef USE_GTK3
  my_signal_connect(mascot->dw_biff, "draw",
		    dw_expose_biff_pix, (gpointer)mascot);
#else
  my_signal_connect(mascot->dw_biff, "expose_event",
		    dw_expose_biff_pix, (gpointer)mascot);
  my_signal_connect(mascot->biff_pix, "expose_event",
		    expose_biff_pix, (gpointer)mascot);
#endif
  my_signal_connect(ebox, "button_press_event",
		    biff_drag_begin, (gpointer)mascot);
  my_signal_connect(ebox, "button_release_event",
		    biff_drag_end, (gpointer)mascot);
  my_signal_connect(ebox, "motion_notify_event",
		    biff_window_motion, (gpointer)mascot);

  gtk_window_resize (GTK_WINDOW(mascot->biff_pix), 1, 1);
  gtk_widget_set_size_request (mascot->dw_biff, 1, 1);
}


void display_biff_balloon(typMascot *mascot)
{
  gchar *text_tip=NULL;

  if(mascot->mail.tooltips_fl){

    if(mascot->mail.count!=0){
      if((mascot->mail.type==MAIL_POP3)||(mascot->mail.type==MAIL_APOP)){
	if(mascot->mail.spam_count!=0){
	  if((mascot->mail.last_f!=NULL)&&(mascot->mail.last_s!=NULL))
	    text_tip=g_strdup_printf(_("You have %d mails (%d SPAMs).\n [Latest Mail]\n  From: %s\n  Subject: %s"),
				     mascot->mail.count,mascot->mail.spam_count,
				     mascot->mail.last_f,mascot->mail.last_s);
	}
	else{
	  if((mascot->mail.last_f!=NULL)&&(mascot->mail.last_s!=NULL))
	    text_tip=g_strdup_printf(_("You have %d mails.\n [Latest Mail]\n  From: %s\n  Subject: %s"),
				     mascot->mail.count,
				     mascot->mail.last_f,mascot->mail.last_s);
	}
	if(text_tip){
	  gtk_widget_set_tooltip_text(mascot->biff_pix,
				      text_tip);
	  g_free(text_tip);
	}
      }
      else{
	text_tip=g_strdup_printf(_("You have %d mails."),mascot->mail.count);
	gtk_widget_set_tooltip_text(mascot->biff_pix,
				    text_tip);
	g_free(text_tip);
      }
    }
    else{
      gtk_widget_set_tooltip_text(mascot->biff_pix,
				  _("You have no mails."));
    }
  }

  if(mascot->mail.status == POP3_ERROR){
    map_biff(mascot, FALSE);

    if(flag_balloon==FALSE){
      mascot->balseq=0;
      mascot->bal_mode=BALLOON_POPERROR;
      DoBalloon(mascot); 
      flag_balloon=TRUE;
      mascot->mail.status = NO_MAIL;
    }
  }
  else if(mascot->mail.status == POP3_SSL_CERT){
    map_biff(mascot, FALSE);
    
#ifdef USE_SSL
    mascot->mail.ssl_cert_res=ssl_manager_verify_cert(mascot);
    if(mascot->mail.ssl_cert_res==SSL_CERT_ACCEPT)
      ResetMailChecker(mascot);
#endif
  }
  else if(mascot->mail.status != NO_MAIL){
    MoveBiffPix(mascot,mascot->x,mascot->y);
    map_biff(mascot, TRUE);
  }
  else{
    map_biff(mascot, FALSE);
  }
  if(mascot->mail.status==NEW_MAIL){
    sound_play(mascot,mascot->mail.sound);

    if((mascot->mail.word)&&(flag_balloon==FALSE)) {
      mascot->balseq=0;
      mascot->bal_mode=BALLOON_MAIL;
      DoBalloon(mascot); 
      flag_balloon=TRUE;
      if((mascot->mail.type==MAIL_POP3)||(mascot->mail.type==MAIL_APOP)){
	mascot->mail.status=KEEP_NEW_MAIL;
      }
    }
  }
}


void remap_biff_pix(typMascot *mascot){
  if(!mascot->mail.flag) return;
  if(mascot->mail.status == (POP3_ERROR|POP3_SSL_CERT)){
    map_biff(mascot, FALSE);
  }
  else if(mascot->mail.status != NO_MAIL){
    MoveBiffPix(mascot,mascot->x,mascot->y);
    map_biff(mascot, TRUE);
  }
  else{
    map_biff(mascot, FALSE);
  }
}

void mail_check(typMascot *mascot){
  switch(mascot->mail.type){     
  case MAIL_LOCAL:
  case MAIL_PROCMAIL:
    mail_check_mbox(mascot);
    break;
  case MAIL_POP3: 
  case MAIL_APOP:
    mail_check_pop3(mascot);
    break;
  case MAIL_QMAIL:
    mail_check_qmail(mascot);
    break;
  }

}


void mail_check_mbox(typMascot *mascot){
    struct stat t;
    DIR *dp;
    struct dirent *entry;
    time_t newest_time=0;
    int filenum=0;
    int mc=0;
    FILE *fp;
    char buf[BUFFSIZE];
    int wo_spam;

    if (!stat(mascot->mail.file, &t)){

      if(t.st_size == 0){ // �ᥤ��ե����륵����������
	mascot->mail.status = NO_MAIL;
	mascot->mail.count = 0;
      }
      else{
	wo_spam = mascot->mail.count - mascot->mail.spam_count;

	if (t.st_size < mascot->mail.size){
	  // mailbox smaller in size; some mails have been deleted
	  mascot->mail.status = OLD_MAIL;
	}
	else if (t.st_atime > t.st_mtime){ 
	  // mailbox read after most recent write
	  mascot->mail.status = OLD_MAIL;
	}
	else if (t.st_size > mascot->mail.size){
	  // mailbox modified after most recent read, and larger in size
	  // this implies the arrival of some new mails
	  mascot->mail.status = NEW_MAIL;
	  // get mail count
	  if((fp=fopen(mascot->mail.file,"r"))!= NULL){
	    do{
	      if(fgets(buf,BUFFSIZE-1,fp)==NULL) break;
	      if(strncmp(buf,"From ",5)==0){
		mc++;
	      }
	    } while (1);
	    fclose(fp);
	    mascot->mail.count = mc;
	  }
	}
	else if ((t.st_size == mascot->mail.size)&&
		 ((mascot->mail.status == NEW_MAIL)||
		  (mascot->mail.status ==KEEP_NEW_MAIL))){
	  mascot->mail.status=KEEP_NEW_MAIL;
	}
	else{
	  mascot->mail.status = NO_MAIL;          // no such mailbox
	  mascot->mail.count = 0;
	}

	if( mc == mascot->mail.spam_count){
	  mascot->mail.status = NO_MAIL; // SPAM�����ΤȤ�
	}
	else if((mc - mascot->mail.spam_count) == wo_spam){
	  mascot->mail.status = KEEP_NEW_MAIL; // �������Τ�SPAM�������ä��Ȥ�
	}
      }
    }
    else{ // �ᥤ��ե����뤬¸�ߤ��ʤ��Ȥ�
      mascot->mail.status = NO_MAIL;  
      mascot->mail.count = 0;
    }
    mascot->mail.size = t.st_size;
}


void mail_check_qmail(typMascot *mascot){
    struct stat t;
    DIR *dp;
    struct dirent *entry;
    gchar *tmp=NULL;
    time_t newest_time=0;
    int filenum=0;
    int wo_spam;

    if ((dp=opendir(mascot->mail.file))==NULL){
      mascot->mail.status = NO_MAIL;  
      mascot->mail.count = 0;
      return;
    }
    
    while((entry=readdir(dp))!=NULL){
      if(entry->d_name[0]!='.'){
	tmp=g_strdup_printf("%s/%s",mascot->mail.file,entry->d_name);
	if (!stat(tmp, &t)){
	  filenum++;
	  if (t.st_mtime>newest_time){ 
	    newest_time=t.st_mtime;
	  }
	}
	if(tmp) g_free(tmp);
	tmp=NULL;
      }
    }
	
    closedir(dp);
	
    if(filenum==0){
      mascot->mail.status = NO_MAIL;
      mascot->mail.count = 0;
    }
    else{
      wo_spam = mascot->mail.count - mascot->mail.spam_count;

      if(newest_time==former_newest){
	mascot->mail.status = KEEP_NEW_MAIL;  
      }
      else if(newest_time>former_newest){
	mascot->mail.status = NEW_MAIL;  
      }
      else{
	mascot->mail.status = OLD_MAIL;  
      }

      if( filenum == mascot->mail.spam_count){
	mascot->mail.status = NO_MAIL; // SPAM�����ΤȤ�
      }
      else if((filenum - mascot->mail.spam_count) == wo_spam){
	mascot->mail.status = KEEP_NEW_MAIL; // �������Τ�SPAM�������ä��Ȥ�
      }
    }
    former_newest=newest_time;
    mascot->mail.count = filenum;
}


void mail_check_pop3(typMascot *mascot){
  if(mascot->mail.pop_child_fl){
    return ;
  }
  mascot->mail.pop_child_fl=TRUE;

  pop_debug_print("Begin POP Thread.\n");

  mascot->mloop=g_main_loop_new(NULL, FALSE);
  mascot->mcancel=g_cancellable_new();
  mascot->mthread=g_thread_new("macopix_get_pop3",
			    thread_get_pop3, (gpointer)mascot);
  g_main_loop_run(mascot->mloop);
  g_thread_join(mascot->mthread);
  g_main_loop_unref(mascot->mloop);
  mascot->mloop=NULL;
  
  mascot->mail.pop_child_fl=FALSE;
}



int get_pop3(typMascot *mascot)
{
  signed int ret;
  int num;
  long size;
  gchar *apop_key;
  int wo_spam;
  int pre_mail_count;

  pre_mail_count = mascot->mail.count;  // store mail count

  pop_debug_print("Conncet %s : %d\n", mascot->mail.pop_server, mascot->mail.pop_port);

  apop_key=g_malloc0(sizeof(gchar)*BUFFSIZE);
  memset(apop_key, '\0', BUFFSIZE);
  ret = popConnect(mascot->mail.pop_server, mascot->mail.pop_port, apop_key, 
		   mascot->mail.ssl_mode, mascot->mail.ssl_nonblock, 
		   mascot->mail.ssl_cert_res,
		   &mascot->mail.ssl_sub, &mascot->mail.ssl_iss,
		   &mascot->mail.ssl_verify, mascot->mail.type);

  if( ret != 0 ){
    if(apop_key) g_free(apop_key);
    if(ret==-(30+4)){
      pop_debug_print("popConnect() [ret=%d] :  Certification is required\n", ret);
      mascot->mail.status = POP3_SSL_CERT;
    }
    else{
      fprintf(stderr, "ERR: popConnect() [ret=%d]\n", ret);
      mascot->mail.status = POP3_ERROR;
      pop3_error(mascot);
    }
    mascot->mail.pop_child_fl=FALSE;
    mascot->mail.pop_readed=TRUE;
    return(-1);
  }

  pop_debug_print("POP LOGIN %s %s %s\n",mascot->mail.pop_id, mascot->mail.pop_pass,
	 apop_key);
  ret = popLogin(mascot->mail.pop_id, mascot->mail.pop_pass, apop_key, mascot->mail.ssl_mode, mascot->mail.type);
  if( ret != 0 ){
    if(apop_key) g_free(apop_key);
    fprintf(stderr, "ERR: popLogin() [ret=%d]\n", ret);
    mascot->mail.status = POP3_ERROR;
    popQuit(mascot->mail.ssl_mode);
    popClose();
    pop3_error(mascot);
    mascot->mail.pop_child_fl=FALSE;
    mascot->mail.pop_readed=TRUE;
    return(-1);
  }

  pop_debug_print("POP STAT\n");

  ret = popStat(&num, &size, mascot->mail.ssl_mode);
  if( ret != 0 ){
    if(apop_key) g_free(apop_key);
    fprintf(stderr, "ERR: popStat() [ret=%d]\n", ret);
    mascot->mail.status = POP3_ERROR;
    popQuit(mascot->mail.ssl_mode);
    popClose();
    pop3_error(mascot);
    mascot->mail.pop_child_fl=FALSE;
    mascot->mail.pop_readed=TRUE;
    return(-1);
  }

  pop_debug_print("mail: %d, size: %ld\n", num, size);
 
  if(num == 0){  // �᡼��̵��
    mascot->mail.status = NO_MAIL;
    mascot->mail.fetched_count = 0;
    mascot->mail.spam_count = 0;
    mascot->mail.pop3_fs_status = POP3_OK_NORMAL;
    if(mascot->mail.last_f) g_free(mascot->mail.last_f);
    mascot->mail.last_f = NULL;
    if(mascot->mail.last_s) g_free(mascot->mail.last_s);
    mascot->mail.last_s =  NULL;
    if(mascot->mail.pop_froms) g_free(mascot->mail.pop_froms);
    mascot->mail.pop_froms = NULL;
  }
  else{
    wo_spam = mascot->mail.count - mascot->mail.spam_count;

    if(num == mascot->mail.count){  // �᡼�������ʤ�
      mascot->mail.status = KEEP_NEW_MAIL;
      mascot->mail.fetched_count = 0;
      mascot->mail.pop3_fs_status = POP3_OK_NORMAL;
    }
    else if(num > mascot->mail.count){  // �᡼��������
      mascot->mail.status = NEW_MAIL;
      fs_get_pop3(num,mascot); // ������SPAM�ο�, pop_froms�����������
      // for WIN32 fs_get_pop3 should not initialize pop_froms
    }
    else{  // �᡼��������äƤ����餹�٤ƿ����᡼��ȸ��ʤ�
      mascot->mail.status = NEW_MAIL;      
      if(mascot->mail.last_f) g_free(mascot->mail.last_f);
      mascot->mail.last_f = NULL;
      if(mascot->mail.last_f) g_free(mascot->mail.last_s);
      mascot->mail.last_s =  NULL;
      if(mascot->mail.pop_froms) g_free(mascot->mail.pop_froms);
      mascot->mail.pop_froms = NULL;
      mascot->mail.count = 0;
      mascot->mail.spam_count = 0;
      fs_get_pop3(num,mascot);
    }

    if( num == mascot->mail.spam_count){
      mascot->mail.status = NO_MAIL; // SPAM�����ΤȤ�
      if(mascot->mail.last_f) g_free(mascot->mail.last_f);
      mascot->mail.last_f = NULL;
      if(mascot->mail.last_s) g_free(mascot->mail.last_s);
      mascot->mail.last_s =  NULL;
      if(mascot->mail.pop_froms) g_free(mascot->mail.pop_froms);
      mascot->mail.pop_froms = NULL;
    }
    else if((num - mascot->mail.spam_count) == wo_spam){
      mascot->mail.status = KEEP_NEW_MAIL; // �������Τ�SPAM�������ä��Ȥ�
    }
  }

  pop_debug_print("data read end\n");

  popQuit(mascot->mail.ssl_mode);
  popClose();

  pop_debug_print("pop quit\n");

  mascot->mail.count = num;

  /*************** from pop3 data read *********/

  // Should be in parent process in UNIX ver

  pop_debug_print("POP Thread: pop3 data read\n");

  if( mascot->mail.status == POP3_ERROR ){  // POP3 status is ERR
    pop3_error(mascot);
  }
#ifdef USE_SSL
  else if( mascot->mail.status == POP3_SSL_CERT){  // SSL Certification
    //mascot->mail.ssl_cert_res=ssl_manager_verify_cert(mascot);
  }
#endif
  else{
    if(mascot->mail.last_f!=NULL){
      if(g_utf8_validate(mascot->mail.last_f,-1,NULL)){
	strip_last_ret(mascot->mail.last_f);
      }
      else{
	mascot->mail.last_f = g_strdup(_("(Decode Error)"));
      }
    }

    if(mascot->mail.last_s!=NULL){
      if(g_utf8_validate(mascot->mail.last_s,-1,NULL)){
	strip_last_ret(mascot->mail.last_s);
      }
      else{
	mascot->mail.last_s = g_strdup(_("(Decode Error)"));
      }
    }
  }

  if(mascot->mail.count == 0){
    mascot->mail.displayed_count = 0;
  }
  else {
    if(mascot->mail.pop3_fs_status == POP3_OK_NORMAL){
      if(pre_mail_count > mascot->mail.count){  //�᡼�븺�ä�
	mascot->mail.displayed_count = mascot->mail.fetched_count; 
      } 
      else { // added or no changed
	mascot->mail.displayed_count += mascot->mail.fetched_count; 
      }

    }
    else if(mascot->mail.pop3_fs_status == POP3_OK_FS_OVER){
      //gchar buf[BUFFSIZE], *tmp_froms;
      gchar *buf=NULL, *tmp_froms=NULL;
      //������̵���� fs clear ���Ƥ褤
      mascot->mail.displayed_count = mascot->mail.fetched_count; 
      
      buf=g_strdup_printf(_(" \n     ***** %d mails are skipped *****\n \n"),
			  mascot->mail.count - mascot->mail.displayed_count);

      tmp_froms=strdup(mascot->mail.pop_froms);
      if(mascot->mail.pop_froms) g_free(mascot->mail.pop_froms);
      mascot->mail.pop_froms=g_strconcat(buf,tmp_froms,NULL);
      if(tmp_froms) g_free(tmp_froms);
      if(buf) g_free(buf);
    }
  }

  mascot->mail.pop_child_fl=FALSE;
  mascot->mail.pop_readed=TRUE;

  pop_debug_print("POP Thread: status = %d\n",mascot->mail.status);
  pop_debug_print("POP Thread: pop3 fs status = %d\n",mascot->mail.pop3_fs_status);
  pop_debug_print("POP Thread: count = %d\n",mascot->mail.count);
  pop_debug_print("POP Thread: fetched count = %d\n",mascot->mail.fetched_count);
  pop_debug_print("POP Thread: disped count = %d\n",mascot->mail.displayed_count);
  pop_debug_print("POP Thread: spam count = %d\n",mascot->mail.spam_count);
  if(mascot->mail.last_f!=NULL)
    pop_debug_print("Last From:       %s\n",mascot->mail.last_f);
  if(mascot->mail.last_s!=NULL)
    pop_debug_print("Last Subject:    %s\n",mascot->mail.last_s);
  if(mascot->mail.pop_froms!=NULL)
    pop_debug_print("data2 = %s\n",mascot->mail.pop_froms);

  pop_debug_print("POP Thread: Done\n");


  /*************** end of  pop3 data read *********/


  if(apop_key) g_free(apop_key);

  return(0);
}


gpointer thread_get_pop3(gpointer gdata){
  typMascot *mascot=(typMascot *)gdata;

  mascot->mabort=FALSE;
  get_pop3(mascot);

  if(mascot->mloop) g_main_loop_quit(mascot->mloop);
}


void thread_cancel_pop3(typMascot *mascot)
{
  if(mascot->mloop){
    g_cancellable_cancel(mascot->mcancel);
    g_object_unref(mascot->mcancel); 

    mascot->mabort=TRUE;

    g_main_loop_quit(mascot->mloop);
  }
}


void pop3_error(typMascot *mascot){
  if( mascot->mail.status < 0){  // POP3 status is ERR
    fprintf(stderr,"POP3 error\n");
    mascot->mail.count = 0;
    mascot->mail.spam_count = 0;
    mascot->mail.fetched_count = 0;
    mascot->mail.displayed_count = 0;
    mascot->mail.status = POP3_ERROR;
    if(mascot->mail.last_f) g_free(mascot->mail.last_f);
    mascot->mail.last_f = NULL;
    if(mascot->mail.last_s) g_free(mascot->mail.last_s);
    mascot->mail.last_s =  NULL;
    if(mascot->mail.pop_froms) g_free(mascot->mail.pop_froms);
    mascot->mail.pop_froms = NULL;
  }
}


// �᡼��ܥå������� From: Subject: �����
gchar * fs_get_mbox(typMascot *mascot){
  FILE *fp;
  char buf[BUFFSIZE], *bufs, *buft;
  gchar *froms=NULL;
  char *p;
  gboolean ed_fl=FALSE, spam_flag;
  fpos_t pos;
  int i; 
  char *f, *s, *sdel;
  
  froms=NULL;
  mascot->mail.spam_count=0;
  
  if((fp=fopen(mascot->mail.file,"r"))!= NULL){
    do{
      if(fgets(buf,BUFFSIZE-1,fp)==NULL) break;
      if(strncmp(buf,"From ",5)==0){  // ���ĤΥ᡼��λϤޤ�
	f=s=NULL;
	spam_flag = FALSE;
	for(;;){
	  if((p=fgets(buf,BUFFSIZE-1,fp))==NULL
	     ||buf[0]=='\n'||buf[0]=='\0')
	    break;
	  if(g_ascii_strncasecmp(buf,"from:",5)==0)
	    {
	      //strcpy(bufs,buf);
	      bufs=g_strdup(buf);
	      while(1){
		fgetpos(fp,&pos);
		if((p=fgets(buf,BUFFSIZE-1,fp))==NULL
		   ||buf[0]=='\n'||buf[0]=='\0'){
		  ed_fl=TRUE;
/* tnaka start */
		  if( strlen(bufs)) {
		    if(f) g_free(f);
		    f = g_strdup(bufs);
		    g_free(bufs);
		    fsetpos(fp,&pos);
		  }
/* tnaka end */
		  break;
		}
		
		if(bufs[strlen(bufs)-1]!='\n'){
		  //strcat(bufs,buf); /* ��Ԥ�200byte�ʾ� */
		  buft=g_strdup(bufs);
		  g_free(bufs);
		  bufs=g_strconcat(buft, buf, NULL);
		  g_free(buft);
		}
		else if((buf[0]==' ') || (buf[0]=='\t')){
		  bufs[strlen(bufs)-1]='\0';
		  sdel = &buf[1];     /* \t ���� */
		  //strcat(bufs,sdel); /* ʣ���� */
		  buft=g_strdup(bufs);
		  g_free(bufs);
		  bufs=g_strconcat(buft, sdel, NULL);
		  g_free(buft);
		}
		else{
		  if(f) g_free(f);
		  f=g_strdup(bufs);
		  g_free(bufs);
		  fsetpos(fp,&pos);
		  break;
		}
	      }
	      if(ed_fl) break;
	    }
	  else if(g_ascii_strncasecmp(buf,"subject:",8)==0)
	    {
	      //strcpy(bufs,buf);
	      bufs=g_strdup(buf);
	      while(1){
		fgetpos(fp,&pos);
		if((p=fgets(buf,BUFFSIZE-1,fp))==NULL
		   ||buf[0]=='\n'||buf[0]=='\0'){
		  ed_fl=TRUE;
/* tnaka start */
		  if( strlen(bufs)) {
		    if(s) g_free(s);
		    s = g_strdup(bufs);
		    g_free(bufs);
		    fsetpos(fp,&pos);
		  }
/* tnaka end */
		  break;
		}
		if(bufs[strlen(bufs)-1]!='\n'){
		  //strcat(bufs,buf); /* ��Ԥ�200byte�ʾ� */
		  buft=g_strdup(bufs);
		  g_free(bufs);
		  bufs=g_strconcat(buft, buf, NULL);
		  g_free(buft);
		}
		else if((buf[0]==' ') || (buf[0]=='\t')){
		  bufs[strlen(bufs)-1]='\0';
		  sdel = &buf[1];     /* \t ���� */
		  //strcat(bufs,sdel); /* ʣ���� */
		  buft=g_strdup(bufs);
		  g_free(bufs);
		  bufs=g_strconcat(buft, sdel, NULL);
		  g_free(buft);
		}
		else{
		  if(s) g_free(s);
		  s=g_strdup(bufs);
		  g_free(bufs);
		  fsetpos(fp,&pos);
		  break;
		}
	      }
	      if(ed_fl) break;
	    } 
	  else if(strncmp(buf, mascot->mail.spam_mark, 
			  strlen(mascot->mail.spam_mark))==0){
	    spam_flag = TRUE;
	    mascot->mail.spam_count++;
	  }
	}
	
	//
	// From: Subject:
	// �ν��ɽ��������
	//
	if ( (!spam_flag) || (!mascot->mail.spam_check) ){
	  if (f) {
	    conv_unmime_header_overwrite(f);
	    froms=my_str_add(froms,f);
	    g_free(f);
	  }
	  else{
	    froms=my_str_add(froms,_("From: (no From: in original)\n"));
	  }
	  if (s) {
	    froms=my_str_add(froms," ");
	    conv_unmime_header_overwrite(s);
	    froms=my_str_add(froms,s);
	    g_free(s);
	  }
	  else{
	    froms=my_str_add(froms,_(" Subject: (no Subject: in original)\n"));
	  }
	}
ed_fl = FALSE;  /* tnaka */
      }
      while (p != NULL && buf[0] != '\n' && buf[0] != '\0')
	p = fgets(buf, BUFFSIZE - 1, fp);
    } while (1);
    fclose(fp);
  }
  return(froms);
}


// MH + Procmail ����
gchar *  fs_get_procmail(typMascot  *mascot){
    FILE *fp,*fp_folder;
    gchar buf[BUFFSIZE],tmp[10], *bufs, *buft;
    gchar *folder_file=NULL,folder_tmp[BUFFSIZE];
    gchar *froms=NULL, *p;
    gboolean ed_fl=FALSE;
    fpos_t pos;
    int i;
    gchar *f, *s, *sdel;


    froms=NULL;

    if((fp=fopen(mascot->mail.file,"r"))==NULL){
      return(NULL);
    }
    
    while(!feof(fp)){
	f=s=NULL;
	if(fgets(buf,BUFFSIZE-1,fp)==NULL) break;
	if(strncmp(buf,"  Folder:",9)==0){
	    sscanf(buf,"%s%s",tmp,folder_tmp);
	    folder_file=g_strconcat(set_mhdir(),G_DIR_SEPARATOR,
				    folder_tmp,NULL);

	    if((fp_folder=fopen(folder_file,"r"))!=NULL){
		while(!feof(fp_folder)){
		    if((p=fgets(buf,BUFFSIZE-1,fp_folder))==NULL
		       ||buf[0]=='\n'||buf[0]=='\0')
			break;
		    if(g_ascii_strncasecmp(buf,"from:",5)==0)
		    {
		      //strcpy(bufs,buf);
		      bufs=g_strdup(buf);
		      while(1){
			fgetpos(fp_folder,&pos);
			if((p=fgets(buf,BUFFSIZE-1,fp_folder))==NULL
			   ||buf[0]=='\n'||buf[0]=='\0'){
			  if(f) g_free(f);
			  f=g_strdup(bufs);
			  g_free(bufs);
			  ed_fl=TRUE;
			  break;
			}
			if(bufs[strlen(bufs)-1]!='\n'){
			  //strcat(bufs,buf); /* ��Ԥ�200byte�ʾ� */
			  buft=g_strdup(bufs);
			  g_free(bufs);
			  bufs=g_strconcat(buft, bufs, NULL);
			  g_free(buft);
			}
			else if((buf[0]==' ') || (buf[0]=='\t')){
			  bufs[strlen(bufs)-1]='\0';
			  sdel = &buf[1];     /* \t ���� */
			  //strcat(bufs,sdel); /* ʣ���� */
			  buft=g_strdup(bufs);
			  g_free(bufs);
			  bufs=g_strconcat(buft, sdel, NULL);
			  g_free(buft);
			}
			else{
			  if(f) g_free(f);
			  f=g_strdup(bufs);
			  g_free(bufs);
			  fsetpos(fp_folder,&pos);
			  break;
			}
		      }
		      if(ed_fl) break;
		    }
		    else if(g_ascii_strncasecmp(buf,"subject:",8)==0){
		      //strcpy(bufs,buf);
		      bufs=g_strdup(buf);
		      while(1){
			fgetpos(fp_folder,&pos);
			if((p=fgets(buf,BUFFSIZE-1,fp_folder))==NULL
			   ||buf[0]=='\n'||buf[0]=='\0'){
			  ed_fl=TRUE;
			  if(s) g_free(s);
			  s=g_strdup(bufs);
			  g_free(bufs);
			  break;
			}
			if(bufs[strlen(bufs)-1]!='\n'){
			  //strcat(bufs,buf); /* ��Ԥ�200byte�ʾ� */
			  buft=g_strdup(bufs);
			  g_free(bufs);
			  bufs=g_strconcat(buft, buf, NULL);
			  g_free(buft);
			}
			else if((buf[0]==' ') || (buf[0]=='\t')){
			  bufs[strlen(bufs)-1]='\0';
			  sdel = &buf[1];     /* \t ���� */
			  //strcat(bufs,sdel); /* ʣ���� */
			  buft=g_strdup(bufs);
			  g_free(bufs);
			  bufs=g_strconcat(buft, sdel, NULL);
			  g_free(buft);
			}
			else{
			  if(s) g_free(s);
			  s=g_strdup(bufs);
			  g_free(bufs);
			  fsetpos(fp_folder,&pos);
			  break;
			}
		      }
		      if(ed_fl) break;
		    } 
		}
		fclose(fp_folder);
		//
		// From: Subject:
		// �ν��ɽ��������
		//
		if (f) {
		  conv_unmime_header_overwrite(f);
		  froms=my_str_add(froms,f);
		  g_free(f);
		}
		else{
		  froms=my_str_add(froms,_("From: (no From: in original)\n"));
		}
		if (s) {
		  froms=my_str_add(froms," ");
		  conv_unmime_header_overwrite(s);
		  froms=my_str_add(froms,s);
		  g_free(s);
		}
		else{
		  froms=my_str_add(froms,_(" Subject: (no Subject: in original)\n"));
		}
	    }
	}
	if(folder_file) g_free(folder_file);
	folder_file=NULL;
	
    }
    fclose(fp);

    if(folder_file) g_free(folder_file);

    return(froms);
}	


// Qmail ����
gchar * fs_get_qmail(typMascot *mascot){
    FILE *fp_folder;
    gchar buf[BUFFSIZE],tmp[10],*bufs, *buft;
    gchar *folder_file,folder_tmp[BUFFSIZE];
    gchar *froms=NULL, *p;
    gboolean ed_fl=FALSE, spam_flag;
    fpos_t pos;
    int i;
    DIR *dp;
    struct dirent *entry;

    gchar *f, *s, *sdel;

    mascot->mail.spam_count=0;
    froms=NULL;

    if ((dp=opendir(mascot->mail.file))==NULL){
      return(NULL);
    }	
    

    while((entry=readdir(dp))!=NULL){
	f=s=NULL;
	if(entry->d_name[0]!='.'){
	    sprintf(folder_file,"%s/%s",mascot->mail.file,entry->d_name);

	    if((fp_folder=fopen(folder_file,"r"))!=NULL){
	        spam_flag = FALSE;
                while(!feof(fp_folder)){
		  if((p=fgets(buf,BUFFSIZE-1,fp_folder))==NULL
		     ||buf[0]=='\n'||buf[0]=='\0')
		    break;

		  if(strncmp(buf, mascot->mail.spam_mark, 
			     strlen(mascot->mail.spam_mark))==0){
		    spam_flag = TRUE;
		    mascot->mail.spam_count++;
		  }
		}
		if( (spam_flag) && (mascot->mail.spam_check) ){
		  fclose(fp_folder);
		  continue;
		}

		rewind(fp_folder);
		while(!feof(fp_folder)){
		    if((p=fgets(buf,BUFFSIZE-1,fp_folder))==NULL
		       ||buf[0]=='\n'||buf[0]=='\0')
			break;
		    if(g_ascii_strncasecmp(buf,"from:",5)==0)
                    {
		      //strcpy(bufs,buf);
		      bufs=g_strdup(buf);
		      while(1){
			fgetpos(fp_folder,&pos);
			if((p=fgets(buf,BUFFSIZE-1,fp_folder))==NULL
			   ||buf[0]=='\n'||buf[0]=='\0'){
			  ed_fl=TRUE;
			  if(f) g_free(f);
			  f=g_strdup(bufs);
			  g_free(bufs);
			  break;
			}
			if(bufs[strlen(bufs)-1]!='\n'){
			  //strcat(bufs,buf); // ��Ԥ�200byte�ʾ�
			  buft=g_strdup(bufs);
			  g_free(bufs);
			  bufs=g_strconcat(buft, buf, NULL);
			  g_free(buft);
			}
			else if((buf[0]==' ') || (buf[0]=='\t')){
			  bufs[strlen(bufs)-1]='\0';
			  sdel = &buf[1];     // \t ����
			  //strcat(bufs,sdel); // ʣ����
			  buft=g_strdup(bufs);
			  g_free(bufs);
			  bufs=g_strconcat(buft, sdel, NULL);
			  g_free(buft);
			}
			else{
			  if(f) g_free(f);
			  f=g_strdup(bufs);
			  g_free(bufs);
			  fsetpos(fp_folder,&pos);
			  break;
			}
		      }
		      if(ed_fl) break;
		    }
		    else if(g_ascii_strncasecmp(buf,"subject:",8)==0)
                    {
		      //strcpy(bufs,buf);
		      bufs=g_strdup(buf);
		      while(1){
			fgetpos(fp_folder,&pos);
			if((p=fgets(buf,BUFFSIZE-1,fp_folder))==NULL
			   ||buf[0]=='\n'||buf[0]=='\0'){
			  if(s) g_free(s);
			  s=g_strdup(bufs);
			  g_free(bufs);
			  ed_fl=TRUE;
			  break;
			}
			if(bufs[strlen(bufs)-1]!='\n'){
			  //strcat(bufs,buf); /* ��Ԥ�200byte�ʾ� */
			  buft=g_strdup(bufs);
			  g_free(bufs);
			  bufs=g_strconcat(buft,buf,NULL);
			  g_free(buft);
			}
			else if((buf[0]==' ') || (buf[0]=='\t')){
			  bufs[strlen(bufs)-1]='\0';
			  sdel = &buf[1];     /* \t ���� */
			  //strcat(bufs,sdel); /* ʣ���� */
			  buft=g_strdup(bufs);
			  g_free(bufs);
			  bufs=g_strconcat(buft,sdel,NULL);
			  g_free(buft);
			}
			else{
			  if(s) g_free(s);
			  s=g_strdup(bufs);
			  g_free(bufs);
			  fsetpos(fp_folder,&pos);
			  break;
			}
		      }
		      if(ed_fl) break;
		    }
		}
		fclose(fp_folder);
		//
		// From: Subject:
		// �ν��ɽ��������
		//
		if (f) {
		  conv_unmime_header_overwrite(f);
		  froms=my_str_add(froms,f);
		  g_free(f);
		}
		else{
		  froms=my_str_add(froms,_("From: (no From: in original)\n"));
		}
		if (s) {
		    froms=my_str_add(froms," ");
		    conv_unmime_header_overwrite(s);
		    froms=my_str_add(froms, s);
		    g_free(s);
		}
		else{
		  froms=my_str_add(froms,_(" Subject: (no Subject: in original)\n"));
		}

	    }
	}
	
    }
    closedir(dp);

    return(froms);
}


// pop3 server ���� From: Subject: �����
void fs_get_pop3(int num, typMascot *mascot){
  int funcret;
  char buffer_header[HEADER_MAX][POP_MAX_LINE], *bufs, *buft;
  char buffer[POP_MAX_LINE];
  int mail_cnt, header, header_line, i, j;
  int mail_cnt_start;
  char *f, *s, *sdel;
  int disp=0;
  gboolean spam_flag;

  pop_debug_print("fs read num = %d mail_count = %d\n", 
	  num, mascot->mail.count);

  if((num - mascot->mail.count) > mascot->mail.pop_max_fs){  //overflow
    mail_cnt_start = num - mascot->mail.pop_max_fs +1;
    mascot->mail.fetched_count = mascot->mail.pop_max_fs;
    mascot->mail.pop3_fs_status = POP3_OK_FS_OVER;
  }
  else {    // under
    mail_cnt_start = mascot->mail.count +1;
    mascot->mail.fetched_count = num - mascot->mail.count;
    mascot->mail.pop3_fs_status = POP3_OK_NORMAL;
  }

  pop_debug_print("fs read num = %d mail_cnt_start = %d\n", 
	  num, mail_cnt_start); 
  pop_debug_print("fs mail_fetched = %d\n", mascot->mail.fetched_count); 

  for(mail_cnt = mail_cnt_start; mail_cnt <= num; mail_cnt++){

    sprintf(buffer, "TOP %d %d\r\n", mail_cnt, 0);  // get header only

    funcret = popWriteLine(buffer, mascot->mail.ssl_mode);
    if( funcret != 0 ){
      fprintf(stderr,"write err = %d\n",funcret);
      //#ifndef USE_WIN32
      //mascot->mail.status=POP3_ERROR;
      //#endif
      return;
    }
    for(header=0; header < HEADER_MAX; header++){
      funcret = popReadLine(buffer_header[header], POP_MAX_LINE, mascot->mail.ssl_mode);
      //pop_debug_print("%d %d   %s\n",mail_cnt,header,buffer_header[header]);
      if( funcret != 0 ){
	fprintf(stderr,"read err\n");
	//#ifndef USE_WIN32
	//	mascot->mail.status=POP3_ERROR;
	//#endif
	return;
      }
      if( strcmp(buffer_header[header], ".\r\n") == 0 )      break;
      buffer_header[header][strlen(buffer_header[header])-2]=' ';  // \r delete
    }
    header_line = header-1;

    spam_flag = FALSE;
    if(mascot->mail.spam_check){
      for(header=0; header < header_line; header++){
	if(strncmp(buffer_header[header], mascot->mail.spam_mark, 
		   strlen(mascot->mail.spam_mark))==0){
	  spam_flag = TRUE;
	  mascot->mail.spam_count++;
	  
	  pop_debug_print("SPAM detected = %d\n", mascot->mail.spam_count); 
	}
      }
    }
    if( (spam_flag) && (mascot->mail.spam_check) )  continue;

    f=s=NULL;
    for(header=0; header < header_line; header++){
      if(g_ascii_strncasecmp(buffer_header[header],"from:",5)==0)
      {
	//strcpy(bufs, buffer_header[header]);
	bufs=g_strdup(buffer_header[header]);
	i=1;
	while(1){
	  if((i+header) > header_line){
	    if(f) g_free(f);
	    f=g_strdup(bufs);
	    g_free(bufs);
	    break;
	  }
	  if((buffer_header[i+header][0]==' ') || (buffer_header[i+header][0]=='\t')){
	    bufs[strlen(bufs)-2]='\0';
	    sdel = &buffer_header[i+header][1];     /* \t ���� */
	    //strcat(bufs,sdel); /* ʣ���� */
	    buft=g_strdup(bufs);
	    g_free(bufs);
	    bufs=g_strconcat(buft, sdel, NULL);
	    g_free(buft);
	  }
	  else{
	    if(f) g_free(f);
	    f=g_strdup(bufs);
	    g_free(bufs);
	    break;
	  }
	  i++;
	}
	pop_debug_print("??? ff=%s\n",f);
      }
      else if(g_ascii_strncasecmp(buffer_header[header],"subject:",8)==0)
      {
	//strcpy(bufs, buffer_header[header]);
	bufs=g_strdup(buffer_header[header]);
	j=1;
	while(1){
	  if((j+header) > header_line){
	    if(s) g_free(s);
	    s=g_strdup(bufs);
	    g_free(bufs);
	    break;
	  }
	  if((buffer_header[j+header][0]==' ') || (buffer_header[j+header][0]=='\t')){
	    bufs[strlen(bufs)-2]='\0';
	    sdel = &buffer_header[j+header][1];     // \t ����
	    //strcat(bufs,sdel); // ʣ����
	    buft=g_strdup(bufs);
	    g_free(bufs);
	    bufs=g_strconcat(buft, sdel, NULL);
	    g_free(buft);
	  }
	  else{
	    if(s) g_free(s);
	    s=g_strdup(bufs);
	    g_free(bufs);
	    break;
	  }
	  j++;
	}
	pop_debug_print("??? ss=%s\n",s);
      }
    }
    

    if (f) {
      conv_unmime_header_overwrite(f);
      {
	gint n;
	for(n=4;n>1;n--){
	  if((f[strlen(f)-n]==0x0D)&&(f[strlen(f)-n+1]==0x0A)){
	    f[strlen(f)-n]=0x20;
	    f[strlen(f)-n+1]=0x20;
	  }
	}
      }
      mascot->mail.pop_froms=my_str_add(mascot->mail.pop_froms,f);
      if(mascot->mail.last_f) g_free(mascot->mail.last_f);
      mascot->mail.last_f=g_strdup(f+strlen("From: "));
      strip_last_ret(mascot->mail.last_f);
      if(f) g_free(f);
    }
    else{
      mascot->mail.pop_froms=my_str_add(mascot->mail.pop_froms,
					_("From: (no From: in original)\n"));
    }
    if (s) {
      mascot->mail.pop_froms=my_str_add(mascot->mail.pop_froms," ");
      conv_unmime_header_overwrite(s);
      {
	gint n;
	for(n=4;n>1;n--){
	  if((s[strlen(s)-n]==0x0D)&&(s[strlen(s)-n+1]==0x0A)){
	    s[strlen(s)-n]=0x20;
	    s[strlen(s)-n+1]=0x20;
	  }
	}
      }
      pop_debug_print("s=%s\n",s);

      mascot->mail.pop_froms=my_str_add(mascot->mail.pop_froms,s);
      if(mascot->mail.last_s) g_free(mascot->mail.last_s);
      mascot->mail.last_s=g_strdup(s+strlen("Subject: "));
      strip_last_ret(mascot->mail.last_s);
      if(s) g_free(s);
    }
    else{
      pop_debug_print("s=!!!! No Subject !!!!\n");
      mascot->mail.pop_froms=my_str_add(mascot->mail.pop_froms,
					_(" Subject: (no Subject: in original)\n"));
    }
  }
  
  pop_debug_print("fs_get_pop3 end\n");
}



gchar * my_str_add(gchar *src_str, gchar *add_str){
  gchar *ret;
  if(!src_str){
    ret=g_strdup(add_str);
  }
  else{
    ret=g_strconcat(src_str, add_str, NULL);
    g_free(src_str);
  }

  return(ret);
}


void make_fs_max(GtkWidget *widget, typMascot *mascot){
  GtkWidget *label;
  gchar tmp_fs_max[256];
  gchar tmp_fs[8];

  if(mascot->mail.displayed_count != mascot->mail.count){
    if(mascot->mail.spam_count!=0){
      sprintf(tmp_fs_max,
	      _("Displaying the newest %d mails out of %ds. [%d SPAMs are excluded.]"),
	      mascot->mail.displayed_count,
	      mascot->mail.count, mascot->mail.spam_count);
    }
    else{
      sprintf(tmp_fs_max,
	      _("Displaying the newest %d mails out of %ds."),
	      mascot->mail.displayed_count,
	      mascot->mail.count);
    }
  }
  else{
    if(mascot->mail.spam_count!=0){
      sprintf(tmp_fs_max,_("You have %d new mails.  [%d SPAMs are excluded.]"),
	      mascot->mail.count, mascot->mail.spam_count);
    }
    else{
      sprintf(tmp_fs_max,_("You have %d new mails."),
	      mascot->mail.count);
    }
  }
  label=gtkut_label_new(tmp_fs_max);
  gtkut_pos(label, POS_END, POS_START);
  gtkut_table_attach (widget, label, 0, 5, 1, 2,
		      GTK_FILL, GTK_SHRINK , 0, 0);
}


// �ᥤ���忮�ꥹ�Ȥ�����
void create_biff_dialog(typMascot *mascot)
{
  GtkWidget *biff_tbl;
  GtkWidget *biff_text;
  GtkWidget *button;
  GtkWidget *biff_scroll;
  gchar *tmp_froms=NULL;
  gchar *tmp;
  gchar *fp_1, *fp_2;
  gchar *p;
  gchar *buf_unmime;
  gchar *err_msg;
  GtkTextBuffer *text_buffer;
  GtkTextIter start_iter, end_iter;
  GtkTextMark *end_mark;
  gint i_fs=0;

  // Win���ۤϽŤ��Τ����Expose���٥�����򤹤٤ƽ������Ƥ���
  while (my_main_iteration(FALSE));

  mascot->flag_menu=TRUE;

  
  mascot->biff_main = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  text_buffer = gtk_text_buffer_new(NULL);

  gtk_widget_set_size_request (mascot->biff_main, mascot->mail.win_width, 
			       mascot->mail.win_height);
  if(mascot->mail.type==(MAIL_POP3|MAIL_APOP)){
    tmp=g_strconcat(_("MaCoPiX : Arrived mail list"),"  [",
		    mascot->mail.pop_server,"]",NULL);
  }
  else{
    tmp=g_strdup(_("MaCoPiX : Arrived mail list"));
  }
  gtk_window_set_title(GTK_WINDOW(mascot->biff_main), tmp);
  g_free(tmp);
  gtk_widget_realize(mascot->biff_main);
  my_signal_connect(mascot->biff_main,"destroy",close_biff,
		    (gpointer)mascot);
  gtk_container_set_border_width (GTK_CONTAINER (mascot->biff_main), 5);
  
  // 6x3�Υơ��֥�
  biff_tbl = gtkut_table_new (6, 3, FALSE, 0, 0, 0);
  gtk_container_add (GTK_CONTAINER (mascot->biff_main), biff_tbl);

  biff_scroll = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(biff_scroll),
                                 GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

  biff_text = gtk_text_view_new_with_buffer (text_buffer);
  gtk_text_view_set_editable (GTK_TEXT_VIEW (biff_text), FALSE);
  gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW (biff_text), FALSE);

  gtk_container_add(GTK_CONTAINER(biff_scroll), biff_text);

  gtkut_table_attach_defaults (biff_tbl, biff_scroll, 0, 5, 0, 1);


  switch(mascot->mail.type){
  case MAIL_LOCAL: 
    tmp_froms=fs_get_mbox(mascot);
    break;
  case MAIL_POP3: 
  case MAIL_APOP: 
    tmp_froms=g_strdup(mascot->mail.pop_froms);
    make_fs_max(biff_tbl, mascot);
    break;
  case MAIL_QMAIL:
    tmp_froms=fs_get_qmail(mascot);
    break;
  case MAIL_PROCMAIL:
    tmp_froms=fs_get_procmail(mascot);
    break;
  }


  

  gtk_text_buffer_create_tag (text_buffer, "underline",
			      "underline", PANGO_UNDERLINE_SINGLE, NULL);
  gtk_text_buffer_create_tag (text_buffer, "big_gap_after_line",
			      "pixels_below_lines", 5, NULL);
  gtk_text_buffer_create_tag (text_buffer, "heading",
			      "weight", PANGO_WEIGHT_BOLD,
			      //"size", 15 * PANGO_SCALE,
			      NULL);
  
  gtk_text_buffer_create_tag (text_buffer, "color0",
#ifdef USE_GTK3
			      "paragraph-background-rgba",
#else
			      "paragraph-background-gdk",
#endif
			      mascot->colfsbg0,
#ifdef USE_GTK3
			      "foreground-rgba",
#else
			      "foreground-gdk",
#endif
			      mascot->colfsfg0,
			      NULL);
  gtk_text_buffer_create_tag (text_buffer, "color1",
#ifdef USE_GTK3
			      "paragraph-background-rgba",
#else
			      "paragraph-background-gdk",
#endif
			      mascot->colfsbg1,
#ifdef USE_GTK3
			      "foreground-rgba",
#else
			      "foreground-gdk",
#endif
			      mascot->colfsfg1,
			      NULL);
  gtk_text_buffer_get_start_iter(text_buffer, &start_iter);

  if((strlen(tmp_froms)<5)||(mascot->mail.status == NO_MAIL)){
    err_msg=g_strdup(_("   === Failed to get From/Subject list of arrived mails. ==="));
    g_locale_to_utf8(err_msg,-1,NULL,NULL,NULL);
    gtk_text_buffer_insert (text_buffer, &start_iter, err_msg,-1);
    
    g_free(err_msg);
  }
  else{
    p=(gchar *)strtok(tmp_froms,"\n");
    do{
      buf_unmime=g_strdup(p);
      
      if(g_ascii_strncasecmp(buf_unmime," subject:",9)==0){
	gtk_text_buffer_insert_with_tags_by_name (text_buffer, &start_iter,
						  _("Subject: "), -1,
						  "heading",
						  "big_gap_after_line",
						  (i_fs%2) ? "color1" : "color0",
						  NULL);
	if(g_utf8_validate(buf_unmime+9,-1,NULL)){
	  gtk_text_buffer_insert_with_tags_by_name (text_buffer, &start_iter,
	  					    buf_unmime+9, -1,
	  					    "big_gap_after_line",
						    (i_fs%2) ? "color1" : "color0",
	  					    NULL);
	}
	else{
	  gtk_text_buffer_insert (text_buffer, &start_iter, 
				  "", -1);
	}
	i_fs++;
      }
      else if(g_ascii_strncasecmp(buf_unmime,"from:",5)==0){
	gtk_text_buffer_insert_with_tags_by_name (text_buffer, &start_iter,
						  _("From: "), -1,
						  "heading",
						  (i_fs%2) ? "color1" : "color0",
						  NULL);
	if(g_utf8_validate(buf_unmime+5,-1,NULL)){
	  gtk_text_buffer_insert_with_tags_by_name (text_buffer, &start_iter,
						    buf_unmime+5, -1,
						    (i_fs%2) ? "color1" : "color0",
						    NULL);
	}
	else{
	  gtk_text_buffer_insert (text_buffer, &start_iter, 
				  _("(invalid/no from in original)"), -1);
	}
      }
      else{
	if(g_utf8_validate(buf_unmime,-1,NULL)){
	  gtk_text_buffer_insert (text_buffer, &start_iter, buf_unmime, -1);
	}
      }
      gtk_text_buffer_insert (text_buffer, &start_iter, "\n", -1);
      if(buf_unmime) g_free(buf_unmime);
    }while((p=(gchar *)strtok(NULL,"\n"))!=NULL);
  }

  gtk_text_buffer_get_end_iter(text_buffer, &end_iter);
  gtk_text_buffer_place_cursor(text_buffer, &end_iter);
  end_mark= gtk_text_buffer_create_mark(text_buffer, "end", &end_iter, FALSE);
  gtk_text_view_scroll_to_mark(GTK_TEXT_VIEW(biff_text),
			       end_mark, 0.0, FALSE, 0.0, 0.0);
  gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(biff_text),
			       &end_iter,0.0, FALSE,0.0, 0.0); 

  
  button=gtkut_button_new_with_icon(_("Start Mailer"),
#ifdef USE_GTK3
				    "mail-read"
#else
				    GTK_STOCK_EXECUTE
#endif				     
				    );
  gtkut_table_attach(biff_tbl, button, 0, 1, 2, 3,
		     GTK_FILL,GTK_SHRINK,0,0);
  my_signal_connect(button,"clicked",mailer_start,
		    (gpointer)mascot);


  button=gtkut_button_new_with_icon(_("Close"),
#ifdef USE_GTK3
				    "window-close"
#else
				    GTK_STOCK_CLOSE
#endif				     
				    );
  gtkut_table_attach(biff_tbl, button, 4, 5, 2, 3,
		     GTK_FILL,GTK_SHRINK,0,0);
  my_signal_connect(button,"clicked",close_biff,
		    (gpointer)mascot);
  
  gtk_widget_show_all(mascot->biff_main);
  
  gdkut_flush(mascot->win_main);
}


static void close_biff(GtkWidget *w, gpointer gdata)
{
  typMascot *mascot = (typMascot *)gdata;
  
  while (my_main_iteration(FALSE));
  gtk_widget_destroy(GTK_WIDGET(mascot->biff_main));
  while (my_main_iteration(FALSE));

  mascot->flag_menu=FALSE;
}


// Biff win����Υᥤ�鵯ư
static void mailer_start(GtkWidget *w, gpointer gdata)
{
  typMascot *mascot = (typMascot *)gdata;
  
  while (my_main_iteration(FALSE));
  gtk_widget_destroy(GTK_WIDGET(mascot->biff_main));
  while (my_main_iteration(FALSE));
 
  mascot->flag_menu=FALSE;

  ext_play(mascot, mascot->mail.mailer);
}


gchar* set_mhdir(){
  FILE *fp;
  gchar *c=NULL, buf[256],*mhd=NULL, *tmp;
    
  c=g_strconcat(g_get_home_dir(),PROCMAILRC,NULL);

  if((fp=fopen(c,"r"))!=NULL){
    while(!feof(fp)){
      if(fgets(buf,256-1,fp)==NULL) break;
      if(strncmp(buf,"MAILDIR=",8)==0){
	tmp=buf+8;
	if(strncmp(tmp,"$HOME",5)==0){
	  mhd=g_strconcat(g_get_home_dir(),tmp+5,NULL);
	}
	else{
	  mhd=g_strdup(tmp);
	}
	break;
      }
    }
  }
    
  if(mhd==NULL){
    mhd=g_strconcat(g_get_home_dir(),MH_MAIL_DIR,NULL);
  }
  if(mhd[strlen(mhd)-1]=='\n') mhd[strlen(mhd)-1]='\0';

  if(c) g_free(c);
  
  return mhd;
}

void mail_arg_init(typMascot *mascot){
  mascot->mail.pop_froms=NULL;
  former_newest=0;
}

void strip_last_ret(gchar *p){
  if(p[strlen(p)-1]=='\n') p[strlen(p)-1]='\0';
}


/*
#ifndef USE_WIN32
void kill_pop3(){
  pid_t child_pid=0;

  if(pop3_pid) kill(pop3_pid,SIGKILL);

  do{
    int child_ret;
    child_pid=waitpid(pop3_pid, &child_ret,WNOHANG);
  } while(child_pid>0);
}
#endif
*/

#endif // USE_BIFF


