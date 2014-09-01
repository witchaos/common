#include "stdafx.h"

#include <stdio.h>
#include <ctype.h>
#include <process.h>

#define __MSG_C__
#include "msg.h"
#include "utils.h"
#include "deal.h"
#include "about.h"
#include "comm.h"
#include "asctable.h"
#include "str2hex.h"
#include "debug.h"
#include "struct/memory.h"
#include "struct/list.h"
#include "../res/resource.h"

#include <SdkLayout.h>
#include <Richedit.h>

static sdklayout* layout;

struct msg_s msg;

static list_s list_head;
CRITICAL_SECTION window_critical_section;

static char* __THIS_FILE__  = __FILE__;

static int set_ctrl_font(const char* name, const char* fs)
{
	int id;
	char* c = strchr((char*)fs,',');
	*c = '\0';
	id = layout_newfont(msg.layout, fs, strtol(c+1, NULL, 10));
	layout_setfont(layout_control(msg.layout, name), id);
	return 0;
}

static void com_init_from_config(void)
{
	if(g_cfg){
		char* v;
		char* k;
		k = "app.title";
		if(config_has(g_cfg, k)){
			v = config_getstr(g_cfg, k, "");
			SetWindowText(msg.hWndMain, v);
			free(v);
		}

		k = "ui.font";
		if(config_has(g_cfg, k)){
			char* comma;
			v = config_getstr(g_cfg, k, "");
			comma = strchr(v, ',');
			*comma='\0';
			layout_deffont(msg.layout, v, strtol(comma+1, NULL, 10));
			free(v);
		}

		k = "app.icon";
		if(config_has(g_cfg, k)){
			HICON hIcon;
			v = config_getstr(g_cfg, k, "");
			hIcon = (HICON)LoadImage(NULL, v, IMAGE_ICON, 48, 48, LR_LOADFROMFILE);
			if(hIcon){
				SendMessage(msg.hWndMain, WM_SETICON, ICON_SMALL, (LPARAM)(hIcon));
				SendMessage(msg.hWndMain, WM_SETICON, ICON_BIG,   (LPARAM)(hIcon));
			}
			free(v);
		}

		k = "ui.recv.edit_char.font";
		if(config_has(g_cfg, k)){
			v = config_getstr(g_cfg, k, "");
			set_ctrl_font("edit_recv_char", v);
			free(v);
		}

		k = "ui.recv.edit_hex.font";
		if(config_has(g_cfg, k)){
			v = config_getstr(g_cfg, k, "");
			set_ctrl_font("edit_recv_hex", v);
			free(v);
		}
	}
}


//��Ϣ�ṹ���ʼ��, ���캯��
int init_msg(void)
{
	memset(&msg, 0, sizeof(msg));
	msg.run_app           = run_app;
	msg.on_create         = on_create;
	msg.on_close          = on_close;
	msg.on_destroy        = on_destroy;
	msg.on_command        = on_command;
	msg.on_timer          = on_timer;
	msg.on_device_change  = on_device_change;
	msg.on_setting_change = on_setting_change;
	msg.on_size           = on_size;
	msg.on_app			  = on_app;
	return 1;
}

/**************************************************
��  ��:RecvEditWndProc@16
��  ��:������EDIT���������,ȡ�����ѡ���ı�ʱ�Բ�����ı���ɵĸ���
��  ��:
����ֵ:
˵  ��:��EM_SETSEL �� EM_REPLACESEL����䲻����ѡ��
**************************************************/
WNDPROC OldRecvEditWndProc = NULL;
LRESULT CALLBACK RecvEditWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	static int fEnableSelect = 1;
	switch(uMsg)
	{
	case EM_SETSEL:
		fEnableSelect = 0;
		return CallWindowProc(OldRecvEditWndProc, hWnd, uMsg, wParam, lParam);
	case EM_REPLACESEL:
		{
			LRESULT ret;
			ret = CallWindowProc(OldRecvEditWndProc, hWnd, uMsg, wParam, lParam);
			fEnableSelect = 1;
			return ret;
		}
	case WM_LBUTTONDBLCLK:
	case WM_LBUTTONDOWN:
	case WM_MOUSEMOVE:
		if(fEnableSelect==0 || comm.fShowDataReceived&&msg.hComPort!=NULL){
			if(uMsg==WM_LBUTTONDBLCLK || uMsg==WM_LBUTTONDOWN){
				SetFocus(hWnd);
			}
			return 0;
		}
		break;
	case WM_CONTEXTMENU:
		if(comm.fShowDataReceived&&msg.hComPort!=NULL)
			return 0;
		else 
			break;
	}
	return CallWindowProc(OldRecvEditWndProc, hWnd, uMsg, wParam, lParam);
}

WNDPROC OldRecv2EditWndProc = NULL;
LRESULT CALLBACK Recv2EditWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	static int fEnableSelect = 1;
	switch(uMsg)
	{
	case EM_SETSEL:
		fEnableSelect = 0;
		return CallWindowProc(OldRecv2EditWndProc, hWnd, uMsg, wParam, lParam);
	case EM_REPLACESEL:
		{
			LRESULT ret;
			ret = CallWindowProc(OldRecv2EditWndProc, hWnd, uMsg, wParam, lParam);
			fEnableSelect = 1;
			return ret;
		}
	case WM_LBUTTONDBLCLK:
	case WM_LBUTTONDOWN:
	case WM_MOUSEMOVE:
		if(fEnableSelect==0 || comm.fShowDataReceived&&msg.hComPort!=NULL){
			if(uMsg==WM_LBUTTONDBLCLK || uMsg==WM_LBUTTONDOWN){
				SetFocus(hWnd);
			}
			return 0;
		}
		break;
	case WM_CONTEXTMENU:
		if(msg.hComPort != NULL && comm.fShowDataReceived){
			POINT pt;
			HMENU hEditMenu;
			hEditMenu = GetSubMenu(LoadMenu(msg.hInstance,MAKEINTRESOURCE(IDR_MENU_EDIT_RECV)),0);
			GetCursorPos(&pt);
			CheckMenuItem(hEditMenu,MENU_EDIT_CHINESE,MF_BYCOMMAND|(comm.fDisableChinese?MF_UNCHECKED:MF_CHECKED));
			CheckMenuItem(hEditMenu,MENU_EDIT_SEND_INPUT_CHAR,MF_BYCOMMAND|(comm.fEnableCharInput?MF_CHECKED:MF_UNCHECKED));
			TrackPopupMenu(hEditMenu,TPM_LEFTALIGN,pt.x,pt.y,0,msg.hWndMain,NULL);
			return 0;
		}else{
			break;
		}
	case WM_CHAR:
		{
			if(comm.fEnableCharInput && comm.fShowDataReceived && msg.hComPort!=NULL){
				int result;
				result = deal.send_char_data((char)wParam);
				if(result==0){
					utils.msgbox(msg.hWndMain,MB_ICONERROR,"","�����ַ�����ʧ��!");
				}
				return 0;
			}
			goto _def_proc;
		}
	}
_def_proc:
	return CallWindowProc(OldRecv2EditWndProc, hWnd, uMsg, wParam, lParam);
}

#define _SETRESULT(_msg,_result,_msgret) \
	case _msg:SetDlgMsgResult(hWnd,_msg,_result);return _msgret
LRESULT CALLBACK MainWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uMsg)
	{
		//---
		_SETRESULT(WM_CLOSE,msg.on_close(),1);
		_SETRESULT(WM_DESTROY,msg.on_destroy(),1);
		_SETRESULT(WM_COMMAND,msg.on_command((HWND)lParam, LOWORD(wParam), HIWORD(wParam)),1);
		_SETRESULT(WM_DEVICECHANGE,msg.on_device_change(wParam,(DEV_BROADCAST_HDR*)lParam),1);
		_SETRESULT(WM_TIMER,msg.on_timer((int)wParam),1);
		_SETRESULT(WM_SETTINGCHANGE,msg.on_setting_change(),1);
		_SETRESULT(WM_SIZE,msg.on_size(LOWORD(lParam),HIWORD(lParam)),1);
		_SETRESULT(WM_APP,msg.on_app(uMsg,wParam,lParam),1);
		//---
	case WM_LBUTTONDOWN:
		SendMessage(msg.hWndMain,WM_NCLBUTTONDOWN,HTCAPTION,0);
		return 0;
	case WM_INITDIALOG:
		msg.on_create(hWnd,(HINSTANCE)GetModuleHandle(NULL));
		return FALSE;
	case WM_VSCROLL:
	case WM_HSCROLL:
		layout_scroll(layout, uMsg, wParam, lParam);
		return 0;

	default:return 0;
	}
}
#undef _SETRESULT

//!!!һ��ʼ�Ͳ�Ӧ��ʹ��DialogBoxParam��,ʹ��DialogBoxParamȴ��ʹ����DestroyWindow,�Ǻ�,�Ҵ���
int run_app(void)
{
	HINSTANCE hInstance = GetModuleHandle(NULL);
	//return DialogBoxParam(hInstance,MAKEINTRESOURCE(IDD_DLG_MAIN),NULL,(DLGPROC)MainWndProc,(LPARAM)hInstance);
	return (int)CreateDialogA(hInstance,MAKEINTRESOURCE(IDD_DLG_MAIN),NULL,(DLGPROC)MainWndProc);
}

int on_timer(int id)
{
	if(id==TIMER_ID_THREAD){
		debug_out(("����on_timer\n"));
		comm.close(1);
		comm.update((int*)-1);
	}
	KillTimer(msg.hWndMain,TIMER_ID_THREAD);
	return 0;
}
int on_create(HWND hWnd, HINSTANCE hInstance)
{
	HICON hIcon = NULL;
	LOGFONT lf={0};

	//��ʼ�����
	msg.hWndMain = hWnd;
	msg.hInstance = hInstance;
	msg.hEditRecv = GetDlgItem(hWnd, IDC_EDIT_RECV);
	//msg.hEditRecv2 = GetDlgItem(hWnd, IDC_EDIT_RECV2);
	msg.hEditSend = GetDlgItem(hWnd, IDC_EDIT_SEND);
	msg.hComPort = NULL;

	msg.hEditRecv2 = CreateWindowEx(0, RICHEDIT_CLASS, NULL, 
		WS_CHILD | WS_VISIBLE | WS_HSCROLL | WS_VSCROLL |
		ES_MULTILINE | ES_WANTRETURN | ES_AUTOHSCROLL | ES_AUTOVSCROLL |
		ES_READONLY,
		0, 0, 100, 100, hWnd, (HMENU)IDC_EDIT_RECV2, hInstance, 0);

	//�Ѵ����ƶ�����Ļ����
	utils.center_window(hWnd,NULL);
	//������ͼ��
	hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1));
	SendMessage(msg.hWndMain, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
	SetWindowText(hWnd,COMMON_NAME_AND_VERSION);
	SetFocus(GetDlgItem(hWnd,IDC_BTN_OPEN));
	//SetClassLong(hWnd,gcl_)

	//���ؿ�ݼ���
	msg.hAccel = LoadAccelerators(msg.hInstance,MAKEINTRESOURCE(IDR_ACCELERATOR1));
	
	//�ı���ʾ�������뷢�ͻ����С
	SendDlgItemMessage(msg.hWndMain, IDC_EDIT_RECV, EM_SETLIMITTEXT, (WPARAM)COMMON_RECV_BUF_SIZE, 0);
	SendDlgItemMessage(msg.hWndMain, IDC_EDIT_SEND, EM_SETLIMITTEXT, (WPARAM)COMMON_SEND_BUF_SIZE, 0);
	SendDlgItemMessage(msg.hWndMain, IDC_EDIT_RECV2,EM_SETLIMITTEXT, (WPARAM)COMMON_RECV_BUF_SIZE, 0);
	OldRecvEditWndProc=(WNDPROC)SetWindowLongPtr(GetDlgItem(msg.hWndMain, IDC_EDIT_RECV), GWL_WNDPROC, (LONG)RecvEditWndProc);
	OldRecv2EditWndProc=(WNDPROC)SetWindowLongPtr(GetDlgItem(msg.hWndMain, IDC_EDIT_RECV2), GWL_WNDPROC, (LONG)Recv2EditWndProc);
	//ShowWindow(msg.hEditRecv2,FALSE);
	//TODO:
	comm.init();
	comm.update((int*)-1);
	if(ComboBox_GetCount(GetDlgItem(hWnd,IDC_CBO_CP))==0)
		deal.update_status("û���κο��õĴ���!");

	memory.manage_mem(MANMEM_INITIALIZE,NULL);//�ڴ������������������Լ����ڴ����get_mem������֮ǰ��ʼ��
	list->init(&list_head);
	InitializeCriticalSection(&window_critical_section);
	deal.do_buf_send(SEND_DATA_ACTION_INIT,NULL);

	do{
		layout = layout_new(hWnd, MAKEINTRESOURCE(IDR_RCDATA2),hInstance);
		msg.layout = layout;
		layout_visible(layout_control(layout, "recv_wnd_recv"), FALSE);
		layout_resize(layout,NULL);
	}while(0);

	com_init_from_config();

	ShowWindow(hWnd,SW_SHOWNORMAL);

	return 0;
}

int on_close(void)
{
	if(msg.hComPort!=NULL){
		int ret;
		ret = utils.msgbox(msg.hWndMain,MB_ICONEXCLAMATION|MB_OKCANCEL, "��ʾ",
			"��ǰ�����ڵ���״̬, ��ȷ��Ҫ�˳���?");
		if(ret != IDOK)
			return 0;
		if(comm.close(0)==0)
			return 0;
	}
	deal.do_buf_send(SEND_DATA_ACTION_FREE,NULL);
	deal.do_buf_recv(NULL, 0,3);
	SendMessage(msg.hWndMain,WM_APP+0,2,0);
	memory.manage_mem(MANMEM_FREE,NULL);
	DeleteCriticalSection(&window_critical_section);
	DestroyWindow(msg.hWndMain);
	return 0;
}

int on_destroy(void)
{
	msg.hWndMain = NULL;

	PostQuitMessage(0);
	return 0;
}

int on_command(HWND hWndCtrl, int id, int codeNotify)
{
	if(!hWndCtrl && !codeNotify){//Menu
		switch(id)
		{
		//Menu - Other
		case MENU_OTHER_HELP:about.show();break;
		case MENU_OTHER_NEWVERSION:about.update();break;
		case MENU_OTHER_ASCII:ShowAsciiTable();break;
		case MENU_OTHER_CALC:MessageBox(msg.hWndMain, "not implemented!","",0);break;
		case MENU_OTHER_NOTEPAD:ShellExecute(NULL,"open","notepad",NULL,NULL,SW_SHOWNORMAL);break;
		case MENU_OTHER_DEVICEMGR:ShellExecute(NULL,"open","devmgmt.msc",NULL,NULL,SW_SHOWNORMAL);break;
		case MENU_OTHER_MONITOR:MessageBox(msg.hWndMain, "not implemented!","",0);break;
		case MENU_OTHER_DRAW:MessageBox(msg.hWndMain, "not implemented!","",0);break;
		//Menu - More Settings
		case MENU_MORE_TIMEOUTS:comm.show_timeouts();break;
		case MENU_MORE_DRIVER:comm.hardware_config();break;
		case MENU_MORE_PINCTRL:comm.show_pin_ctrl();break;
		case MENU_OTHER_STR2HEX:ShowStr2Hex();break;
		//Menu - EditBox
		case MENU_EDIT_CHINESE:comm.switch_disp();break;
		case MENU_EDIT_SEND_INPUT_CHAR:comm.switch_send_input_char();break;
		case MENU_EDIT_EMPTY: SetWindowText(msg.hEditRecv2, "");break; //���ܵ�����add_text��ͻ
		}
		return 0;
	}
	if(!hWndCtrl && codeNotify==1)
	{
		switch(id)
		{
		case IDACC_SEND:		SendMessage(msg.hWndMain,WM_COMMAND,MAKEWPARAM(IDC_BTN_SEND,BN_CLICKED),(LPARAM)GetDlgItem(msg.hWndMain,IDC_BTN_SEND));break;
		case IDACC_OPEN:		SendMessage(msg.hWndMain,WM_COMMAND,MAKEWPARAM(IDC_BTN_OPEN,BN_CLICKED),(LPARAM)GetDlgItem(msg.hWndMain,IDC_BTN_OPEN));break;
		case IDACC_CLRCOUNTER:	SendMessage(msg.hWndMain,WM_COMMAND,MAKEWPARAM(IDC_BTN_CLR_COUNTER,BN_CLICKED),(LPARAM)GetDlgItem(msg.hWndMain,IDC_BTN_CLR_COUNTER));break;
		case IDACC_STOPDISP:	SendMessage(msg.hWndMain,WM_COMMAND,MAKEWPARAM(IDC_BTN_STOPDISP,BN_CLICKED),(LPARAM)GetDlgItem(msg.hWndMain,IDC_BTN_STOPDISP));break;
		}
		return 0;
	}

	switch(id)
	{
	case IDC_RADIO_SEND_CHAR:
	case IDC_RADIO_SEND_HEX:
	case IDC_RADIO_RECV_CHAR:
	case IDC_RADIO_RECV_HEX:
	case IDC_CHECK_IGNORE_RETURN:
	case IDC_CHECK_USE_ESCAPE_CHAR:
		comm.set_data_fmt();
		return 0;
	case IDC_BTN_STOPDISP:
		{
			if(comm.fShowDataReceived){//�������ͣ��ʾ,���뵽��ͣģʽ
				SetWindowText(hWndCtrl, "������ʾ(&D)");
				comm.fShowDataReceived = 0;
			}else{//����˼�����ʾ, ���뵽��ʾģʽ
				SetWindowText(hWndCtrl, "��ͣ��ʾ(&D)");
				comm.fShowDataReceived = 1;
			}

			if(comm.fShowDataReceived){
				if(!deal.last_show){
					//�ȴ��û���Ӧ�Ĺ�����, ���������߳�, ��Ȼ�����г�ͻ
					ResetEvent(deal.hEventContinueToRead);
					deal.do_check_recv_buf();
					SetEvent(deal.hEventContinueToRead);
				}
			}
			deal.last_show = comm.fShowDataReceived;
		
			deal.update_savebtn_status();
			return 0;
		}
	case IDC_BTN_COPY_RECV:
	case IDC_BTN_COPY_SEND:
		{
			char* buffer = NULL;
			int length = 0;
			if(id==IDC_BTN_COPY_RECV&&comm.fShowDataReceived&&msg.hComPort!=NULL){
				utils.msgbox(msg.hWndMain,MB_ICONEXCLAMATION,COMMON_NAME,
					"����ʾģʽ�²��������ƽ���������!\n"
					"���� ֹͣ��ʾ �л�����ͣ��ʾģʽ.");
				return 0;
			}
			length = GetWindowTextLength(GetDlgItem(msg.hWndMain, id==IDC_BTN_COPY_RECV?(comm.data_fmt_recv==DATA_FMT_HEX?IDC_EDIT_RECV:IDC_EDIT_RECV2):IDC_EDIT_SEND));
			if(length == 0){
				MessageBeep(MB_ICONINFORMATION);
				//Ӧlin0119Ҫ��, ȡ�����Ƴɹ�����ʾ
				//utils.msgbox(msg.hWndMain,MB_ICONQUESTION,COMMON_NAME,"ʲô��û��,���븴��ɶ?");
				return 0;
			}
			buffer = (char*)GET_MEM(length+1);
			if(buffer==NULL) return 0;

			GetDlgItemText(msg.hWndMain, id==IDC_BTN_COPY_RECV?(comm.data_fmt_recv==DATA_FMT_HEX?IDC_EDIT_RECV:IDC_EDIT_RECV2):IDC_EDIT_SEND, buffer, length+1);
			if(!utils.set_clip_data(buffer)){
				utils.msgbox(msg.hWndMain,MB_ICONHAND, NULL, "����ʧ��!");
			}else{
				//Ӧlin0119Ҫ��, ȡ�����Ƴɹ�����ʾ
 				//utils.msgbox(msg.hWndMain,MB_ICONINFORMATION, COMMON_NAME, 
 				//	"�Ѹ��� %s�� ���ݵ�������!", id==IDC_BTN_COPY_RECV?"����":"����");
			}
			memory.free_mem((void**)&buffer,NULL);
			return 0;
		}
	case IDC_BTN_LOADFILE:
		{
			RECT rc;
			HWND h = GetDlgItem(msg.hWndMain,IDC_CBO_CP);
			GetWindowRect(h,&rc);
			comm.load_from_file();
			//2013-7-5 ��ʱBUG����
			//��֪��Ϊʲô����ѡ��ComboBox������С����ʧ��....
			SetWindowPos(h,0,0,0,rc.right-rc.left,rc.bottom-rc.top,SWP_NOMOVE|SWP_NOZORDER);
		}		
		return 0;
	case IDC_BTN_SAVEFILE:
		comm.save_to_file();
		return 0;
	case IDC_BTN_HELP:
		{
			HMENU hMenu;
			POINT pt;
			hMenu=GetSubMenu(LoadMenu(msg.hInstance,MAKEINTRESOURCE(IDR_MENU_OTHER)),0);
			GetCursorPos(&pt);
			TrackPopupMenu(hMenu,TPM_LEFTALIGN,pt.x,pt.y,0,msg.hWndMain,NULL);
			return 0;
		}
	case IDC_EDIT_RECV:
		if(codeNotify==EN_ERRSPACE || codeNotify==EN_MAXTEXT){
			int ret;
			ret=utils.msgbox(msg.hWndMain,MB_ICONEXCLAMATION|MB_YESNOCANCEL,NULL,
				"���ջ���������, ����ս��������ݻ��Ǳ���,�ֻ���ȡ��?\n\n"
				"��ѡ��   ��:��Ҫ�󱣴����������,����ս���������\n"
				"��ѡ��   ��:���������ݽ��ᱻ���,���ݲ�������\n"
				"��ѡ�� ȡ��:���������ݽ�������,�����ݽ��޷�����ʾ\n\n"
				"���ǽ�����û������Ҫ����,�����벻Ҫ���ȡ��!");
			if(ret==IDYES){
				if(comm.save_to_file()){
					msg.on_command(NULL,IDC_BTN_CLR_RECV,BN_CLICKED);
				}else{
					int a;
					a=utils.msgbox(msg.hWndMain,MB_ICONQUESTION|MB_YESNO,COMMON_NAME,"����û�б�����,Ҫ������ս��ջ�����ô?");
					if(a==IDYES){
						msg.on_command(NULL,IDC_BTN_CLR_RECV,BN_CLICKED);
					}
				}
			}else if(ret==IDNO){
				msg.on_command(NULL,IDC_BTN_CLR_RECV,BN_CLICKED);
			}else if(ret==IDCANCEL){
				//ȡ��...
			}
		}
		return 0;
	case IDC_EDIT_SEND:
		if(codeNotify==EN_ERRSPACE || codeNotify==EN_MAXTEXT){
			utils.msgbox(msg.hWndMain,MB_ICONEXCLAMATION|MB_YESNO, NULL, "���ͻ�������!");
		}else if(codeNotify == EN_CHANGE){
			if(comm.fAutoSend){
				deal.cancel_auto_send(0);
				utils.msgbox(msg.hWndMain,MB_ICONINFORMATION,COMMON_NAME,"���ڷ��������Ѹı�, �Զ��ط���ȡ��!");
			}
		}
		return 0;
	case IDC_BTN_SEND:
		deal.do_send(0);
		return 0;
	case IDC_BTN_CLR_COUNTER:
		//δ���ͼ�������Ҫ����
		InterlockedExchange((long volatile*)&comm.cchSent, 0);
		InterlockedExchange((long volatile*)&comm.cchReceived, 0);
		deal.update_status(NULL);
		return 0;
	case IDC_BTN_CLR_SEND:
		deal.cancel_auto_send(0);
		SetDlgItemText(msg.hWndMain,IDC_EDIT_SEND,"");
		return 0;
	case IDC_BTN_CLR_RECV:
		{
			//�Ҿ���ͬʱ���16�������ݺ��ַ��������б�Ҫ��, ������, ��������˵~
			int r = utils.msgbox(msg.hWndMain, MB_ICONQUESTION|MB_YESNOCANCEL, 
				"��ʾ",
				"��ѡ�������������������!\n\n"
				"��\t����� %s ����\n"
				"��\t���ʮ���������ݺ��ַ�����\n"
				"ȡ��\tȡ�����β���",
				comm.data_fmt_recv?"ʮ������":"�ַ�"
				);
			switch(r)
			{
			case IDCANCEL:
				return 0;
				break;
			case IDNO:
				SetDlgItemText(msg.hWndMain,IDC_EDIT_RECV,"");
				SetDlgItemText(msg.hWndMain,IDC_EDIT_RECV2,"");
				break;
			case IDYES:
				SetDlgItemText(msg.hWndMain,comm.data_fmt_recv==DATA_FMT_HEX?IDC_EDIT_RECV:IDC_EDIT_RECV2,"");
				break;
			}
			//
			if(comm.data_fmt_recv==DATA_FMT_HEX) InterlockedExchange((long volatile*)&comm.data_count,0);
			return 0;
		}
	case IDC_CHK_TOP:
		{
			int flag = IsDlgButtonChecked(msg.hWndMain, IDC_CHK_TOP);
			SetWindowPos(msg.hWndMain,flag?HWND_TOPMOST:HWND_NOTOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE);
			BringWindowToTop(msg.hWndMain);
			return 0;
		}
	case IDC_CHECK_SIMPLE:
		{
			int flag = !IsDlgButtonChecked(msg.hWndMain, IDC_CHECK_SIMPLE);

			if(!flag){ // ���ģʽ
				layout_visible(layout_control(layout, "recv_btns"), FALSE);
				layout_visible(layout_control(layout, "simple_mode_help_btn"), TRUE);
				layout_visible(layout_control(layout, "simple_mode_panel"), TRUE);
			}
			else{
				layout_visible(layout_control(layout, "simple_mode_panel"), FALSE);
				layout_visible(layout_control(layout, "recv_btns"), TRUE);
			}

			layout_visible(layout_control(layout, "send_wnd"), flag);
			layout_visible(layout_control(layout, "send_btns"), flag);
			layout_visible(layout_control(layout, "auto_send"), flag);
			layout_visible(layout_control(layout, "send_fmt"), flag);

			SendMessage(msg.hWndMain, WM_SIZE, 0, 0);
			return 0;
		}
	case IDC_CHK_AUTO_SEND:
		deal.check_auto_send();
		return 0;
	case IDC_BTN_OPEN:
		{
			if(comm.fCommOpened){
				if(comm.close(0)){
					comm.update((int*)-1);
				}
			}else{
				comm.open();
			}
			deal.update_savebtn_status();
			return 0;
		}
	case IDC_BTN_MORE_SETTINGS:
		{
			POINT pt;
			HMENU hMenu;
			GetCursorPos(&pt);
			hMenu=GetSubMenu(LoadMenu(msg.hInstance,MAKEINTRESOURCE(IDR_MENU_MORE)),0);
			TrackPopupMenu(hMenu,TPM_LEFTALIGN,pt.x,pt.y,0,msg.hWndMain,NULL);
			return 0;
		}
	case IDC_CBO_CP:
		{
			static RECT rc;
			static char is_there_any_item;
			if(codeNotify == CBN_DROPDOWN){
				GetWindowRect(hWndCtrl,&rc);
				ShowWindow(msg.hEditRecv,FALSE);
				ShowWindow(msg.hEditRecv2,FALSE);
				ShowWindow(GetDlgItem(msg.hWndMain,IDC_STATIC_RECV),FALSE);
				SetWindowPos(hWndCtrl,0,0,0,rc.right-rc.left+300,rc.bottom-rc.top,SWP_NOMOVE|SWP_NOZORDER);
				if(ComboBox_GetCount(hWndCtrl)==0){
					ComboBox_AddString(hWndCtrl,"< û �� �� �� �� �� �� �� �� �� �� ! >  �� �� ˢ �� �� ��");
					is_there_any_item = 0;
				}else{
					is_there_any_item = 1;
				}
				//utils.msgbox(0,NULL,(char*)comm.update((int*)(16+ComboBox_GetCurSel(hWndCtrl))));
				return 0;
			}else if(codeNotify == CBN_SELENDOK || codeNotify==CBN_SELENDCANCEL){
				SetWindowPos(hWndCtrl,0,0,0,rc.right-rc.left,rc.bottom-rc.top,SWP_NOMOVE|SWP_NOZORDER);
				ShowWindow(comm.data_fmt_recv==DATA_FMT_HEX?msg.hEditRecv:msg.hEditRecv2,TRUE);
				ShowWindow(GetDlgItem(msg.hWndMain,IDC_STATIC_RECV),TRUE);
				if(is_there_any_item==0){
					ComboBox_ResetContent(hWndCtrl);
					comm.update((int*)-1);
				}
				return 0;
			}
			break;
		}
	case IDC_CBO_BR:
		{
			if(codeNotify == CBN_SELCHANGE){
				int count=ComboBox_GetCount(hWndCtrl);
				if(ComboBox_GetCurSel(hWndCtrl)==count-1){
					int value;
					extern BOOL GetNewBR(HWND hParent,int* value);

					if(GetNewBR(msg.hWndMain,&value))
					{
						char s[128];
						sprintf(s,"%d",value);
						ComboBox_InsertString(hWndCtrl,count-1,s);
						ComboBox_SetCurSel(hWndCtrl,count-1);
					}else{
						ComboBox_SetCurSel(hWndCtrl,0);
					}
					return 0;
				}
			}
			return 0;
		}
	}
	return 0;
}
/**************************************************
��  ��:on_device_change@8
��  ��:���豸�����ı��⴮���豸�ĸĶ�
��  ��:	event - �豸�¼�
		pDBH - DEV_BROADCAST_HDR*
����ֵ:��MSDN
˵  ��:��û�п��������ظ��Ĵ���, ��ľ��!!!
**************************************************/
#if _MSC_VER > 1200			//compatible with vc6.0
#define  strnicmp _strnicmp
#endif
int on_device_change(WPARAM event, DEV_BROADCAST_HDR* pDBH)
{
	if(msg.hComPort==NULL){
		if(event==DBT_DEVICEARRIVAL){
			if(pDBH->dbch_devicetype == DBT_DEVTYP_PORT){
				DEV_BROADCAST_PORT* pPort = (DEV_BROADCAST_PORT*)pDBH;
				char* name = &pPort->dbcp_name[0];
				if(strnicmp("COM",name,3)==0){
					int com_id;
					char buff[32];
					extern HWND hComPort;
					_snprintf(buff,sizeof(buff),"�Ѽ�⵽�����豸 %s �Ĳ���!",name);
					deal.update_status(buff);
					com_id = atoi(name+3);
					if(comm.update((int*)&com_id))
						ComboBox_SetCurSel(hComPort,com_id);
					SetFocus(GetDlgItem(msg.hWndMain, IDC_EDIT_SEND));
				}
			}
		}else if(event==DBT_DEVICEREMOVECOMPLETE){
			if(pDBH->dbch_devicetype==DBT_DEVTYP_PORT){
				DEV_BROADCAST_PORT* pPort=(DEV_BROADCAST_PORT*)pDBH;
				char* name = &pPort->dbcp_name[0];
				if(strnicmp("COM",name,3)==0){
					char buff[32];
					extern HWND hComPort;
					_snprintf(buff,sizeof(buff),"�����豸 %s ���Ƴ�!",name);
					deal.update_status(buff);
					comm.update((int*)-1);
					if(ComboBox_GetCount(hComPort))
						ComboBox_SetCurSel(hComPort,0);
				}
			}
		}
	}
	return TRUE;
}

int on_setting_change(void)
{
	if(msg.hComPort == NULL){
		extern HWND hComPort;
		comm.update((int*)-1);
		if(ComboBox_GetCount(hComPort)){
			ComboBox_SetCurSel(hComPort,0);
		}
	}
	return 0;
}

int on_size(int width,int height)
{
	layout_resize(layout, NULL);
	return 0;
}

int on_app(UINT uMsg,WPARAM wParam,LPARAM lParam)
{
	switch(uMsg-WM_APP)
	{
	case 0:
		EnterCriticalSection(&window_critical_section);
		if(wParam==0){
			WINDOW_ITEM* pitem = (WINDOW_ITEM*)GET_MEM(sizeof(WINDOW_ITEM));
			pitem->hWnd = (HWND)lParam;
			list->insert_tail(&list_head,&pitem->entry);
		}else if(wParam==1){
			list_s* p = NULL;
			for(p=list_head.next; p!=&list_head; p=p->next){
				WINDOW_ITEM* pitem =  list_data(p,WINDOW_ITEM,entry);
				if(pitem->hWnd == (HWND)lParam){
					list->remove(&list_head,p);
					memory.free_mem((void**)&pitem,NULL);
					break;
				}
			}
		}else if(wParam==2){
			while(!list->is_empty(&list_head)){
				WINDOW_ITEM* pitem = list_data(list->remove_head(&list_head),WINDOW_ITEM,entry);
				DestroyWindow(pitem->hWnd);
				memory.free_mem((void**)&pitem,NULL);
			}
		}
		LeaveCriticalSection(&window_critical_section);
		return 0;
	}
	return 0;
}