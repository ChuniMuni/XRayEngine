#include "stdafx.h"
#include "MainMenu.h"
#include "UI/UIDialogWnd.h"
#include "ui/UIMessageBoxEx.h"
#include "../xrEngine/xr_IOConsole.h"
#include "../xrEngine/IGame_Level.h"
#include "../xrEngine/CameraManager.h"
#include "xr_Level_controller.h"
#include "ui\UITextureMaster.h"
#include "ui\UIXmlInit.h"
#include <dinput.h>
#include "ui\UIBtnHint.h"
#include "UICursor.h"
#include "gamespy/GameSpy_Full.h"
#include "gamespy/GameSpy_HTTP.h"
#include "gamespy/GameSpy_Available.h"
#include "gamespy/CdkeyDecode/cdkeydecode.h"
#include "string_table.h"
#include "../xrCore/os_clipboard.h"

#include <shellapi.h>
#pragma comment(lib, "shell32.lib")


#include "object_broker.h"

//#define DEMO_BUILD

string128	ErrMsgBoxTemplate	[]	= {
		"message_box_invalid_pass",
		"message_box_invalid_host",
		"message_box_session_full",
		"message_box_server_reject",
		"message_box_cdkey_in_use",
		"message_box_cdkey_disabled",
		"message_box_cdkey_invalid",
		"message_box_different_version",
		"message_box_gs_service_not_available",
		"message_box_sb_master_server_connect_failed",
		"msg_box_no_new_patch",
		"msg_box_new_patch",
		"msg_box_patch_download_error",		
		"msg_box_patch_download_success",
		"msg_box_connect_to_master_server",
		"msg_box_kicked_by_server",
		"msg_box_error_loading",
		"message_box_download_level"
};

extern bool b_shniaganeed_pp;

CMainMenu*	MainMenu()	{return (CMainMenu*)g_pGamePersistent->m_pMainMenu; };
//----------------------------------------------------------------------------------
#define INIT_MSGBOX(_box, _template)	{ _box = xr_new<CUIMessageBoxEx>(); _box->InitMessageBox(_template);}
//----------------------------------------------------------------------------------

CMainMenu::CMainMenu	()
{
	m_Flags.zero					();
	m_startDialog					= NULL;
	m_screenshotFrame				= u32(-1);
	g_pGamePersistent->m_pMainMenu	= this;
	if (Device.b_is_Ready)			OnDeviceCreate();  	
	ReadTextureInfo					();
	CUIXmlInit::InitColorDefs		();
	g_btnHint						= NULL;
	m_deactivated_frame				= 0;	
	
	m_sPatchURL						= "";
	m_pGameSpyFull					= NULL;

	m_sPDProgress.IsInProgress		= false;
	m_downloaded_mp_map_url._set	("");

	//---------------------------------------------------------------
	m_NeedErrDialog					= ErrNoError;
	m_start_time					= 0;

	GetPlayerNameFromRegistry		();
	GetCDKeyFromRegistry			();

	if(!g_dedicated_server)
	{
		g_btnHint						= xr_new<CUIButtonHint>();
		m_pGameSpyFull					= xr_new<CGameSpy_Full>();
		
		for (u32 i=0; i<u32(ErrMax); i++)
		{
			CUIMessageBoxEx*			pNewErrDlg;
			INIT_MSGBOX					(pNewErrDlg, ErrMsgBoxTemplate[i]);
			m_pMB_ErrDlgs.push_back		(pNewErrDlg);
		}

		m_pMB_ErrDlgs[PatchDownloadSuccess]->AddCallback("button_yes", MESSAGE_BOX_YES_CLICKED, CUIWndCallback::void_function(this, &CMainMenu::OnRunDownloadedPatch));
		m_pMB_ErrDlgs[PatchDownloadSuccess]->AddCallback("button_yes", MESSAGE_BOX_OK_CLICKED, CUIWndCallback::void_function(this, &CMainMenu::OnConnectToMasterServerOkClicked));

		m_pMB_ErrDlgs[DownloadMPMap]->AddCallback("button_copy", MESSAGE_BOX_COPY_CLICKED, CUIWndCallback::void_function(this, &CMainMenu::OnDownloadMPMap_CopyURL));
		m_pMB_ErrDlgs[DownloadMPMap]->AddCallback("button_yes", MESSAGE_BOX_YES_CLICKED, CUIWndCallback::void_function(this, &CMainMenu::OnDownloadMPMap));
	}
}

CMainMenu::~CMainMenu	()
{
	xr_delete						(g_btnHint);
	xr_delete						(m_startDialog);
	g_pGamePersistent->m_pMainMenu	= NULL;
	xr_delete						(m_pGameSpyFull);
	delete_data						(m_pMB_ErrDlgs);	
}

void CMainMenu::ReadTextureInfo()
{
	FS_FileSet fset;
	FS.file_list(fset, "$game_config$", FS_ListFiles,"ui\\textures_descr\\*.xml");
	FS_FileSetIt fit	= fset.begin();
	FS_FileSetIt fit_e	= fset.end();

	for( ;fit!=fit_e; ++fit)
	{
    	string_path	fn1, fn2,fn3;
        _splitpath	((*fit).name.c_str(),fn1,fn2,fn3,0);
		strcat(fn3,".xml");

		CUITextureMaster::ParseShTexInfo(fn3);
	}

}

extern ENGINE_API BOOL	bShowPauseString;
extern bool				IsGameTypeSingle();

void CMainMenu::Activate	(bool bActivate)
{
	if (	!!m_Flags.test(flActive) == bActivate)		return;
	if (	m_Flags.test(flGameSaveScreenshot)	)		return;
	if (	(m_screenshotFrame == Device.dwFrame)	||
		(m_screenshotFrame == Device.dwFrame-1) ||
		(m_screenshotFrame == Device.dwFrame+1))	return;

	bool b_is_single				= IsGameTypeSingle();

	if(g_dedicated_server && bActivate) return;

	if(bActivate)
	{
		b_shniaganeed_pp			= true;
		Device.Pause				(TRUE, FALSE, TRUE, "mm_activate1");
		m_Flags.set					(flActive|flNeedChangeCapture,TRUE);

		m_Flags.set					(flRestoreCursor,GetUICursor()->IsVisible());

		if(!ReloadUI())				return;

		m_Flags.set					(flRestoreConsole,Console->bVisible);

		if(b_is_single)	m_Flags.set	(flRestorePause,Device.Paused());

		Console->Hide				();


		if(b_is_single)
		{
			m_Flags.set					(flRestorePauseStr, bShowPauseString);
			bShowPauseString			= FALSE;
			if(!m_Flags.test(flRestorePause))
				Device.Pause			(TRUE, TRUE, FALSE, "mm_activate2");
		}

		//m_startDialog->m_bWorkInPause		= true;
		//StartStopMenu						(m_startDialog,true);

		if(g_pGameLevel)
		{
			if(b_is_single){
				Device.seqFrame.Remove		(g_pGameLevel);
			}
			Device.seqRender.Remove			(g_pGameLevel);
			CCameraManager::ResetPP			();
		};
		Device.seqRender.Add				(this, 4); // 1-console 2-cursor 3-tutorial

	}else{
		m_deactivated_frame					= Device.dwFrame;
		m_Flags.set							(flActive,				FALSE);
		m_Flags.set							(flNeedChangeCapture,	TRUE);

		Device.seqRender.Remove				(this);

		bool b = !!Console->bVisible;
		if(b){
			Console->Hide					();
		}

		IR_Release							();
		if(b){
			Console->Show					();
		}

		StartStopMenu						(m_startDialog,true);
		CleanInternals						();
		if(g_pGameLevel)
		{
			if(b_is_single){
				Device.seqFrame.Add			(g_pGameLevel);

			}
			Device.seqRender.Add			(g_pGameLevel);
		};
		if(m_Flags.test(flRestoreConsole))
			Console->Show			();

		if(b_is_single)
		{
			if(!m_Flags.test(flRestorePause))
				Device.Pause			(FALSE, TRUE, FALSE, "mm_deactivate1");

			bShowPauseString			= m_Flags.test(flRestorePauseStr);
		}	

		if(m_Flags.test(flRestoreCursor))
			GetUICursor()->Show			();

		Device.Pause					(FALSE, TRUE, TRUE, "mm_deactivate2");

		if(m_Flags.test(flNeedVidRestart))
		{
			m_Flags.set			(flNeedVidRestart, FALSE);
			Console->Execute	("vid_restart");
		}
	}
}

bool CMainMenu::ReloadUI()
{
	if(m_startDialog)
	{
		StartStopMenu						(m_startDialog,true);
		CleanInternals						();
	}
	DLL_Pure* dlg = NEW_INSTANCE(TEXT2CLSID("MAIN_MNU"));
	if(!dlg) 
	{
		m_Flags.set				(flActive|flNeedChangeCapture,FALSE);
		return false;
	}
	xr_delete					(m_startDialog);
	m_startDialog				= smart_cast<CUIDialogWnd*>(dlg);
	VERIFY						(m_startDialog);
	m_startDialog->m_bWorkInPause= true;
	StartStopMenu				(m_startDialog,true);

	m_activatedScreenRatio		= (float)Device.dwWidth/(float)Device.dwHeight > (1024.0f/768.0f+0.01f);
	return true;
}

bool CMainMenu::IsActive()
{
	return !!m_Flags.test(flActive);
}

bool CMainMenu::CanSkipSceneRendering()
{
	return IsActive() && !m_Flags.test(flGameSaveScreenshot);
}

//IInputReceiver
static int mouse_button_2_key []	=	{MOUSE_1,MOUSE_2,MOUSE_3};
void	CMainMenu::IR_OnMousePress				(int btn)	
{	
	if(!IsActive()) return;

	IR_OnKeyboardPress(mouse_button_2_key[btn]);
};

void	CMainMenu::IR_OnMouseRelease(int btn)	
{
	if(!IsActive()) return;

	IR_OnKeyboardRelease(mouse_button_2_key[btn]);
};

void	CMainMenu::IR_OnMouseHold(int btn)	
{
	if(!IsActive()) return;

	IR_OnKeyboardHold(mouse_button_2_key[btn]);

};

void	CMainMenu::IR_OnMouseMove(int x, int y)
{
	if(!IsActive()) return;

	if(MainInputReceiver())
		MainInputReceiver()->IR_OnMouseMove(x, y);

};

void	CMainMenu::IR_OnMouseStop(int x, int y)
{
};

void	CMainMenu::IR_OnKeyboardPress(int dik)
{
	if(!IsActive()) return;

	if( is_binded(kCONSOLE, dik) )
	{
		Console->Show();
		return;
	}
	if (DIK_F12 == dik){
		Render->Screenshot();
		return;
	}

	if(MainInputReceiver())
		MainInputReceiver()->IR_OnKeyboardPress( dik);

};

void	CMainMenu::IR_OnKeyboardRelease			(int dik)
{
	if(!IsActive()) return;
	
	if(MainInputReceiver())
		MainInputReceiver()->IR_OnKeyboardRelease(dik);

};

void	CMainMenu::IR_OnKeyboardHold(int dik)	
{
	if(!IsActive()) return;
	
	if(MainInputReceiver())
		MainInputReceiver()->IR_OnKeyboardHold(dik);
};

void CMainMenu::IR_OnMouseWheel(int direction)
{
	if(!IsActive()) return;
	
	if(MainInputReceiver())
		MainInputReceiver()->IR_OnMouseWheel(direction);
}


bool CMainMenu::OnRenderPPUI_query()
{
	return IsActive() && !m_Flags.test(flGameSaveScreenshot) && b_shniaganeed_pp;
}


extern void draw_wnds_rects();
void CMainMenu::OnRender	()
{
	if(m_Flags.test(flGameSaveScreenshot))
		return;

	if(g_pGameLevel)
		Render->Calculate			();

	Render->Render				();
	if(!OnRenderPPUI_query())
	{
		DoRenderDialogs();
		UI()->RenderFont();
		draw_wnds_rects();
	}
}

void CMainMenu::OnRenderPPUI_main	()
{
	if(!IsActive()) return;

	if(m_Flags.test(flGameSaveScreenshot))
		return;

	UI()->pp_start();

	if(OnRenderPPUI_query())
	{
		DoRenderDialogs();
		UI()->RenderFont();
	}

	UI()->pp_stop();
}

void CMainMenu::OnRenderPPUI_PP	()
{
	if ( !IsActive() ) return;

	if(m_Flags.test(flGameSaveScreenshot))	return;

	UI()->pp_start();
	
	xr_vector<CUIWindow*>::iterator it = m_pp_draw_wnds.begin();
	for(; it!=m_pp_draw_wnds.end();++it)
	{
		(*it)->Draw();
	}
	UI()->pp_stop();
}

void CMainMenu::StartStopMenu(CUIDialogWnd* pDialog, bool bDoHideIndicators)
{
	pDialog->m_bWorkInPause = true;
	CDialogHolder::StartStopMenu(pDialog, bDoHideIndicators);
}

//pureFrame
void CMainMenu::OnFrame()
{
	if (m_Flags.test(flNeedChangeCapture))
	{
		m_Flags.set					(flNeedChangeCapture,FALSE);
		if (m_Flags.test(flActive))	IR_Capture();
		else						IR_Release();
	}
	CDialogHolder::OnFrame		();


	//screenshot stuff
	if(m_Flags.test(flGameSaveScreenshot) && Device.dwFrame > m_screenshotFrame  )
	{
		m_Flags.set					(flGameSaveScreenshot,FALSE);
		::Render->Screenshot		(IRender_interface::SM_FOR_GAMESAVE, m_screenshot_name);
		
		if(g_pGameLevel && m_Flags.test(flActive))
		{
			Device.seqFrame.Remove	(g_pGameLevel);
			Device.seqRender.Remove	(g_pGameLevel);
		};

		if(m_Flags.test(flRestoreConsole))
			Console->Show			();
	}

	if(IsActive() || m_sPDProgress.IsInProgress)
		m_pGameSpyFull->Update();

	if(IsActive())
	{
		CheckForErrorDlg();
		bool b_is_16_9	= (float)Device.dwWidth/(float)Device.dwHeight > (1024.0f/768.0f+0.01f);
		if(b_is_16_9 !=m_activatedScreenRatio)
		{
			ReloadUI();
		}
	}
}

void CMainMenu::OnDeviceCreate()
{
}


void CMainMenu::Screenshot(IRender_interface::ScreenshotMode mode, LPCSTR name)
{
	if(mode != IRender_interface::SM_FOR_GAMESAVE)
	{
		::Render->Screenshot		(mode,name);
	}else{
		m_Flags.set					(flGameSaveScreenshot, TRUE);
		strcpy_s(m_screenshot_name,name);
		if(g_pGameLevel && m_Flags.test(flActive)){
			Device.seqFrame.Add		(g_pGameLevel);
			Device.seqRender.Add	(g_pGameLevel);
		};
		m_screenshotFrame			= Device.dwFrame+1;
		m_Flags.set					(flRestoreConsole,		Console->bVisible);
		Console->Hide				();
	}
}

void CMainMenu::RegisterPPDraw(CUIWindow* w)
{
	UnregisterPPDraw				(w);
	m_pp_draw_wnds.push_back		(w);
}

void CMainMenu::UnregisterPPDraw				(CUIWindow* w)
{
	m_pp_draw_wnds.erase(
		std::remove(
			m_pp_draw_wnds.begin(),
			m_pp_draw_wnds.end(),
			w
		),
		m_pp_draw_wnds.end()
	);
}

void CMainMenu::SetErrorDialog					(EErrorDlg ErrDlg)	
{ 
	m_NeedErrDialog = ErrDlg;
};

void CMainMenu::CheckForErrorDlg()
{
	if (m_NeedErrDialog == ErrNoError)	return;
	StartStopMenu						(m_pMB_ErrDlgs[m_NeedErrDialog], false);
	m_NeedErrDialog						= ErrNoError;
};

void CMainMenu::SwitchToMultiplayerMenu()
{
	m_startDialog->Dispatch				(2,1);
};

void CMainMenu::DestroyInternal(bool bForce)
{
	if(m_startDialog && ((m_deactivated_frame < Device.dwFrame+4)||bForce) )
		xr_delete		(m_startDialog);
}

void CMainMenu::OnNewPatchFound(LPCSTR VersionName, LPCSTR URL)
{
	if (m_sPDProgress.IsInProgress) return;
	
	if (m_pMB_ErrDlgs[NewPatchFound])	
	{
		delete_data(m_pMB_ErrDlgs[NewPatchFound]);
		m_pMB_ErrDlgs[NewPatchFound] = NULL;
	}
	if (!m_pMB_ErrDlgs[NewPatchFound])
	{
		INIT_MSGBOX(m_pMB_ErrDlgs[NewPatchFound], "msg_box_new_patch");

		shared_str tmpText;
		tmpText.printf(m_pMB_ErrDlgs[NewPatchFound]->GetText(), VersionName, URL);
		m_pMB_ErrDlgs[NewPatchFound]->SetText(*tmpText);		
	}
	m_sPatchURL = URL;
	
	Register						(m_pMB_ErrDlgs[NewPatchFound]);
	m_pMB_ErrDlgs[NewPatchFound]->AddCallback	("button_yes", MESSAGE_BOX_YES_CLICKED, CUIWndCallback::void_function(this, &CMainMenu::OnDownloadPatch));
	StartStopMenu					(m_pMB_ErrDlgs[NewPatchFound], false);
};

void CMainMenu::OnNoNewPatchFound				()
{
	StartStopMenu(m_pMB_ErrDlgs[NoNewPatch], false);
}

void CMainMenu::OnDownloadPatch(CUIWindow*, void*)
{
	CGameSpy_Available GSA;
	shared_str result_string;
	if (!GSA.CheckAvailableServices(result_string))
	{
		Msg(*result_string);
		return;
	};
	
	LPCSTR fileName = *m_sPatchURL;
	if (!fileName) return;

	string4096 FilePath = "";
	char* FileName = NULL;
	GetFullPathName(fileName, 4096, FilePath, &FileName);

	string_path		fname;
	if (FS.path_exist("$downloads$"))
	{
		FS.update_path(fname, "$downloads$", FileName);
		m_sPatchFileName = fname;
	}
	else
		m_sPatchFileName.printf	("downloads\\%s", FileName);	
	
	m_sPDProgress.IsInProgress	= true;
	m_sPDProgress.Progress		= 0;
	m_sPDProgress.FileName		= m_sPatchFileName;
	m_sPDProgress.Status		= "";

	m_pGameSpyFull->m_pGS_HTTP->DownloadFile(*m_sPatchURL, *m_sPatchFileName);
}

void	CMainMenu::OnDownloadPatchError()
{
	m_sPDProgress.IsInProgress	= false;
	StartStopMenu(m_pMB_ErrDlgs[PatchDownloadError], false);
};

void	CMainMenu::OnDownloadPatchSuccess			()
{
	m_sPDProgress.IsInProgress	= false;
	
	StartStopMenu(m_pMB_ErrDlgs[PatchDownloadSuccess], false);
}

void	CMainMenu::OnSessionTerminate				(LPCSTR reason)
{
	if ( m_NeedErrDialog == SessionTerminate && (Device.dwTimeGlobal - m_start_time) < 8000 )
		return;

	m_start_time = Device.dwTimeGlobal;
	LPCSTR str = CStringTable().translate("ui_st_kicked_by_server").c_str();
	LPSTR		text;

	if ( reason && xr_strlen(reason) && reason[0] == '@' )
	{
		STRCONCAT( text, reason + 1 );
	}
	else
	{
		STRCONCAT( text, str, " ", reason );
	}
	
	m_pMB_ErrDlgs[SessionTerminate]->SetText( text );
	SetErrorDialog(CMainMenu::SessionTerminate);
}

void	CMainMenu::OnLoadError				(LPCSTR module)
{
	LPCSTR str=CStringTable().translate("ui_st_error_loading").c_str();
	string1024 Text;
	strconcat(sizeof(Text),Text,str," ");
	strcat_s(Text,sizeof(Text),module);
	m_pMB_ErrDlgs[LoadingError]->SetText(Text);
	SetErrorDialog(CMainMenu::LoadingError);
}

void	CMainMenu::OnDownloadPatchProgress			(u64 bytesReceived, u64 totalSize)
{
	m_sPDProgress.Progress = (float(bytesReceived)/float(totalSize))*100.0f;
};

extern ENGINE_API string512  g_sLaunchOnExit_app;
extern ENGINE_API string512  g_sLaunchOnExit_params;
extern ENGINE_API string_path	g_sLaunchWorkingFolder;
void	CMainMenu::OnRunDownloadedPatch			(CUIWindow*, void*)
{
	strcpy_s					(g_sLaunchOnExit_app,*m_sPatchFileName);
	strcpy_s					(g_sLaunchOnExit_params,"");
	strcpy_s					(g_sLaunchWorkingFolder, "");
	Console->Execute		("quit");
}

void CMainMenu::CancelDownload()
{
	m_pGameSpyFull->m_pGS_HTTP->StopDownload();
	m_sPDProgress.IsInProgress	= false;
}

void CMainMenu::SetNeedVidRestart()
{
	m_Flags.set(flNeedVidRestart,TRUE);
}

void CMainMenu::OnDeviceReset()
{
	if(IsActive() && g_pGameLevel)
		SetNeedVidRestart();
}

// -------------------------------------------------------------------------------------------------

LPCSTR CMainMenu::AddHyphens( LPCSTR c )
{
	static string64 buf;

	u32 sz = xr_strlen(c);
	u32 j = 0; 

	for ( u32 i = 1; i <= 3; ++i )
	{
		buf[i*5 - 1] = '-';
	}

	for ( u32 i = 0; i < sz; ++i )
	{
		j = i + iFloor(i/4.0f);
		buf[j] = c[i];		
	}
	buf[sz + iFloor(sz/4.0f)] = 0;

	return buf;
}

LPCSTR CMainMenu::DelHyphens( LPCSTR c )
{
	static string64 buf;

	u32 sz = xr_strlen( c );
	u32 sz1 = _min( iFloor(sz/4.0f), 3 );

	u32 j = 0; 
	for ( u32 i = 0; i < sz - sz1; ++i )
	{
		j = i + iFloor( i/4.0f );
		buf[i] = c[j];		
	}
	buf[sz - sz1] = 0;
	
	return buf;
}

extern	void	GetCDKey_FromRegistry(char* CDKeyStr);
extern	void	GetPlayerName_FromRegistry	(char* name, u32 const name_size);
//extern	int VerifyClientCheck(const char *key, unsigned short cskey);

bool CMainMenu::IsCDKeyIsValid()
{
	if (!m_pGameSpyFull || !m_pGameSpyFull->m_pGS_HTTP) return false;
	string64 CDKey = "";
	GetCDKey_FromRegistry(CDKey);

#ifndef DEMO_BUILD
	if (!xr_strlen(CDKey)) return true;
#endif

	int GameID = 0;
	for (int i=0; i<4; i++)
	{
		m_pGameSpyFull->m_pGS_HTTP->xrGS_GetGameID(&GameID, i);
		if (VerifyClientCheck(CDKey, unsigned short (GameID)) == 1)
			return true;
	};	
	return false;
}

bool		CMainMenu::ValidateCDKey					()
{
	if (IsCDKeyIsValid()) return true;
	SetErrorDialog(CMainMenu::ErrCDKeyInvalid);
	return false;
}

void		CMainMenu::Show_CTMS_Dialog				()
{
	if (!m_pMB_ErrDlgs[ConnectToMasterServer]) return;
	if (m_pMB_ErrDlgs[ConnectToMasterServer]->IsShown()) return;
	StartStopMenu(m_pMB_ErrDlgs[ConnectToMasterServer], false);
}

void		CMainMenu::Hide_CTMS_Dialog()
{
	if (!m_pMB_ErrDlgs[ConnectToMasterServer]) return;
	if (!m_pMB_ErrDlgs[ConnectToMasterServer]->IsShown()) return;
	StartStopMenu(m_pMB_ErrDlgs[ConnectToMasterServer], false);
}

void CMainMenu::OnConnectToMasterServerOkClicked(CUIWindow*, void*)
{
	Hide_CTMS_Dialog();
}

LPCSTR CMainMenu::GetGSVer()
{
	static string256	buff;
	static string256	buff2;
	if(m_pGameSpyFull)
	{
		strcpy_s(buff2, m_pGameSpyFull->GetGameVersion(buff));
	}else
	{
		buff[0]		= 0;
		buff2[0]	= 0;
	}

	return buff2;
}

LPCSTR CMainMenu::GetPlayerNameFromRegistry()
{
	string512 name;
	GetPlayerName_FromRegistry( name, sizeof(name) );
	m_player_name._set( name );
	return m_player_name.c_str();
}

LPCSTR CMainMenu::GetCDKeyFromRegistry()
{
	string512 key;
	GetCDKey_FromRegistry( key );
	m_cdkey._set( key );
	return m_cdkey.c_str();
}

void CMainMenu::Show_DownloadMPMap(LPCSTR text, LPCSTR url)
{
	VERIFY( m_pMB_ErrDlgs[DownloadMPMap] );

	m_downloaded_mp_map_url._set( url );

	m_pMB_ErrDlgs[DownloadMPMap]->SetText(text);
	m_pMB_ErrDlgs[DownloadMPMap]->SetTextEditURL(url);

	StartStopMenu(m_pMB_ErrDlgs[DownloadMPMap], false);
}

void CMainMenu::OnDownloadMPMap_CopyURL(CUIWindow* w, void* d)
{
	LPCSTR url = m_downloaded_mp_map_url.c_str();
	os_clipboard::copy_to_clipboard( url );
}

void CMainMenu::OnDownloadMPMap(CUIWindow* w, void* d)
{
	LPCSTR url = m_downloaded_mp_map_url.c_str();
	LPCSTR params = NULL;
	STRCONCAT(params, "/C start ", url);
	ShellExecute(0, "open", "cmd.exe", params, NULL, SW_SHOW);
}
