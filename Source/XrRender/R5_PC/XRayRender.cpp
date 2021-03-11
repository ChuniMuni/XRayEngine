// xrRender_R2.cpp : Defines the entry point for the DLL application.
//
#include "pch.h" 
#pragma comment(lib,"xrEngine.lib")
const char* GGraphicsAPI[] = { TEXT("BearRenderDX11") };
XRayRenderFactory BRenderFactory;
XRayDUInterface  BDUInterface;
#ifdef DEBUG
XRayDebugRender BDebugRender;
#endif
BOOL APIENTRY DllMain( HANDLE hModule, 
                       DWORD  ul_reason_for_call, 
                       LPVOID lpReserved
					 )
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH	:

	{

	}
		//	Can't call CreateDXGIFactory from DllMain
		//if (!xrRender_test_hw())	return FALSE;
		::Render					= &GRenderInterface;
		::RenderFactory				= &BRenderFactory;
		::DU						= &BDUInterface;
		//::vid_mode_token			= inited by HW;
		UIRender					= &GUIRender;
#ifdef DEBUG
		DRender						= &BDebugRender;
#endif
		XRayRenderConsole::Initialize();
//	DEBUG
		/*FS.SubPath(TEXT("%cur_shaders%"));
		FS.SubPath(TEXT("%shaders_cache%"));
		FS.AppendPath(TEXT("%cur_shaders%"), ::Render->getShaderPath(), TEXT("%shaders%"), 0);
		FS.AppendPath(TEXT("%shaders_cache%"), ::Render->getShaderPath(), TEXT("%user%"), 0);
		FS.CreateDirectory(TEXT("%shaders_cache%"), 0);*/
	/*	FS.SubPath(TEXT("%cur_shaders%"));
		FS.SubPath(TEXT("%shaders_cache%"));
		FS.AppendPath(TEXT("%cur_shaders%"), ::Render->getShaderPath(), TEXT("%shaders%"), 0);
		FS.AppendPath(TEXT("%shaders_cache%"), TEXT("r4"), TEXT("%user%"), 0);
		FS.CreateDirectory(TEXT("%shaders_cache%"), 0);
		*/break	;
	case DLL_THREAD_ATTACH	:
	case DLL_THREAD_DETACH	:
	case DLL_PROCESS_DETACH	:
		break;
	}
	return TRUE;
}


extern "C"
{
	bool _declspec(dllexport) SupportsRendering();
};
bool _declspec(dllexport) SupportsRendering()
{
	/*bool result = BearRenderInterface::Initialize(TEXT("bear_render_vulkan1_0"));
	BearRenderInterface::Destroy();
	return result;*/
	return true;
}