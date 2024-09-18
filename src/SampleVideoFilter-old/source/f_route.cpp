#include <windows.h>
#include <commctrl.h>
#include <gdiplus.h>
#include <stdio.h>
#include <vd2/VDXFrame/VideoFilter.h>
#include <vd2/VDXFrame/VideoFilterDialog.h>
#include "../resource.h"
#include "gdiplusheaders.h"

#include "pugixml.hpp"

using namespace Gdiplus;
#pragma comment (lib,"Gdiplus.lib")

extern int g_VFVAPIVersion;

///////////////////////////////////////////////////////////////////////////////

class RouteFilterConfig {
public:
	RouteFilterConfig()
	{
		strcpy(mFile, "c:\\rgmapvideo\\settings.xml");
	}

public:
	char mFile[1024];
};

///////////////////////////////////////////////////////////////////////////////

class RouteFilterDialog : public VDXVideoFilterDialog {
public:
	RouteFilterDialog(RouteFilterConfig& config, IVDXFilterPreview *ifp) : mConfig(config) /*, mifp(ifp)*/ {}

	bool Show(HWND parent) {
		return 0 != VDXVideoFilterDialog::Show(NULL, MAKEINTRESOURCE(IDD_FILTER_ROUTE_DLG), parent);
	}

	virtual INT_PTR DlgProc(UINT msg, WPARAM wParam, LPARAM lParam);

protected:
	bool OnInit();
	bool OnCommand(int cmd);
	void OnDestroy();

	void LoadFromConfig();
	bool SaveToConfig();

	RouteFilterConfig& mConfig;
	RouteFilterConfig mOldConfig;
};

INT_PTR RouteFilterDialog::DlgProc(UINT msg, WPARAM wParam, LPARAM lParam) {
	switch(msg) {
		case WM_INITDIALOG:
			return !OnInit();

		case WM_DESTROY:
			OnDestroy();
			break;

		case WM_COMMAND:
			if (OnCommand(LOWORD(wParam)))
				return TRUE;
			break;
	}

	return FALSE;
}

bool RouteFilterDialog::OnInit() {
	mOldConfig = mConfig;

	LoadFromConfig();

	return false;
}

void RouteFilterDialog::OnDestroy() {
}

bool RouteFilterDialog::OnCommand(int cmd) {
	switch(cmd) {
		case IDOK:
			SaveToConfig();
			EndDialog(mhdlg, true);
			return true;

		case IDCANCEL:
			mConfig = mOldConfig;
			EndDialog(mhdlg, false);
			return true;

		case IDC_BROWSE:
			{
				OPENFILENAME ofn; 
				char szFile[1024];
				// Initialize OPENFILENAME
				ZeroMemory(&ofn, sizeof(ofn));
				ofn.lStructSize = sizeof(ofn);
				ofn.hwndOwner = mhdlg;
				ofn.lpstrFile = szFile;
				// Set lpstrFile[0] to '\0' so that GetOpenFileName does not 
				// use the contents of szFile to initialize itself.
				ofn.lpstrFile[0] = '\0';
				ofn.nMaxFile = sizeof(szFile);
				ofn.lpstrFilter = "All\0*.*\0XML\0*.xml\0";
				ofn.nFilterIndex = 1;
				ofn.lpstrFileTitle = NULL;
				ofn.nMaxFileTitle = 0;
				ofn.lpstrInitialDir = NULL;
				ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

				// Display the Open dialog box. 

				if (GetOpenFileName(&ofn)==TRUE) 
				{
					SetDlgItemText(mhdlg, IDC_FILE, szFile);
					SaveToConfig();
				}
			}
			return true;
	}

	return false;
}

void RouteFilterDialog::LoadFromConfig() 
{
	SetDlgItemText(mhdlg, IDC_FILE, mConfig.mFile);
}

bool RouteFilterDialog::SaveToConfig() 
{
	char file_name[1024];
	UINT retI = GetDlgItemText(mhdlg, IDC_FILE, file_name, 1024);
	retI = strcmp(mConfig.mFile, file_name);
	strcpy_s(mConfig.mFile, 1024, file_name);
    bool ret = retI != 0; 
	return ret;
}

///////////////////////////////////////////////////////////////////////////////

class RouteFilter : public VDXVideoFilter {
public:
    RouteFilter() : 
      m_pMap(NULL), 
      m_pTimeBrush(NULL), m_pTimeFont(NULL),
      m_pPaceBrush(NULL), m_pPaceFont(NULL)
    {
    }
    RouteFilter::RouteFilter(const RouteFilter &f)
    {
        throw NotImplemented;
    }
    virtual ~RouteFilter();

	virtual uint32 GetParams();
    virtual bool Init();
	virtual void Start();
	virtual void Run();
	virtual void End();
	virtual bool Configure(VDXHWND hwnd);
	virtual void GetSettingString(char *buf, int maxlen);
	virtual void GetScriptString(char *buf, int maxlen);

	VDXVF_DECLARE_SCRIPT_METHODS();

protected:
    Bitmap *PrepareRGB32(void* data, uint32 pitch, uint32 w, uint32 h);
	void DrawRoute(Bitmap *bmp, uint32 ms);
    void ApplyRGB32(Bitmap *bmp, void* data, uint32 pitch, uint32 w, uint32 h);

	void ScriptConfig(IVDXScriptInterpreter *isi, const VDXScriptValue *argv, int argc);

	RouteFilterConfig	mConfig;
	GdiplusStartupInput gdiplusStartupInput;
	ULONG_PTR           gdiplusToken;

   	pugi::xml_document  m_SettingsDoc;
    Bitmap             *m_pMap;
    int                 m_StageMapOpaque, m_StageMapX, m_StageMapY, m_StageMapWidth, m_StageMapHeight;

    int32               m_TimeOffset;   //ms
    
    PointF              m_TimePos;
    Font               *m_pTimeFont;
    SolidBrush         *m_pTimeBrush;
   	
    PointF              m_PacePos;
    Font               *m_pPaceFont;
    SolidBrush         *m_pPaceBrush;
    int                 m_AvgTail;

    pugi::xml_document  m_PathDoc;
    pugi::xml_node      m_current_Sample;
};

VDXVF_BEGIN_SCRIPT_METHODS(RouteFilter)
VDXVF_DEFINE_SCRIPT_METHOD(RouteFilter, ScriptConfig, "s")
VDXVF_END_SCRIPT_METHODS()

uint32 RouteFilter::GetParams() {
	if (g_VFVAPIVersion >= 12) {
		switch(fa->src.mpPixmapLayout->format) {
			case nsVDXPixmap::kPixFormat_XRGB8888:
				break;

			default:
				return FILTERPARAM_NOT_SUPPORTED;
		}
	}

	fa->dst.offset = fa->src.offset;
	return FILTERPARAM_SUPPORTS_ALTFORMATS;
}



RouteFilter::~RouteFilter()
{
}

bool RouteFilter::Init()
{
    bool ret = VDXVideoFilter::Init();

    return ret;
}

#define CapsCharToVal(x) (*(x)>'9'?*(x)-'A'+10:*(x)-'0')
#define CapsCharToVal2(x) (CapsCharToVal(x+1)|(CapsCharToVal(x)<<4))

void RouteFilter::Start() 
{
	// Initialize GDI+.
	GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    if (mConfig.mFile[0] != 0)
    {
        pugi::xml_parse_result result = m_SettingsDoc.load_file(mConfig.mFile);
        pugi::xml_node settings = m_SettingsDoc.child("RouteAddSettings");

        //load map file image
        pugi::xml_node map_file = settings.child("MapFile");

        WCHAR file_name[1024];
        mbstowcs(file_name, map_file.child_value(), 1024); 
        m_pMap = Bitmap::FromFile(file_name);

        //setup stage 
        pugi::xml_node stage_map = settings.child("StageMap");
        m_StageMapX = stage_map.attribute("x").as_int();
        m_StageMapY = stage_map.attribute("y").as_int();
        m_StageMapWidth = stage_map.attribute("w").as_int();
        m_StageMapHeight = stage_map.attribute("h").as_int();
        m_StageMapOpaque = stage_map.attribute("opaq").as_int();

        //setup time position 
        pugi::xml_node time_pos = settings.child("Time");
        WCHAR font_name[1024];
        mbstowcs(font_name, time_pos.attribute("font").as_string(), 1024); 
        m_pTimeFont = new Font(font_name, time_pos.attribute("size").as_float());
        const char *str_color = time_pos.attribute("color").as_string();
        BYTE r = CapsCharToVal2(str_color);
        BYTE g = CapsCharToVal2(str_color+2);
        BYTE b = CapsCharToVal2(str_color+4);
        m_pTimeBrush = new SolidBrush(Color(r,g,b)); 
        m_TimePos.X = time_pos.attribute("x").as_float();
        m_TimePos.Y = time_pos.attribute("y").as_float();

        //load path
        pugi::xml_node path_file = settings.child("PathFile");
        result = m_PathDoc.load_file(path_file.child_value());
        m_current_Sample = m_PathDoc.child("Route").child("Segment").first_child();
        m_TimeOffset = path_file.attribute("offset").as_int();

        pugi::xml_node pos_map = settings.child("PosMap");

        pugi::xml_node pace_pos = settings.child("Pace");
        mbstowcs(font_name, pace_pos.attribute("font").as_string(), 1024); 
        m_pPaceFont = new Font(font_name, pace_pos.attribute("size").as_float());
        str_color = pace_pos.attribute("color").as_string();
        r = CapsCharToVal2(str_color);
        g = CapsCharToVal2(str_color+2);
        b = CapsCharToVal2(str_color+4);
        m_pPaceBrush = new SolidBrush(Color(r,g,b)); 
        m_PacePos.X = pace_pos.attribute("x").as_float();
        m_PacePos.Y = pace_pos.attribute("y").as_float();
        m_AvgTail = pace_pos.attribute("avg").as_int();

        pugi::xml_node pulse_pos = settings.child("Pulse");

    }
}

void RouteFilter::End()
{
    if (m_pTimeFont)
    {
        delete m_pTimeFont;
        m_pTimeFont = NULL;
    }
    if (m_pTimeBrush)
    {
        delete m_pTimeBrush;
        m_pTimeBrush = NULL;
    }
    if (m_pPaceFont)
    {
        delete m_pPaceFont;
        m_pPaceFont = NULL;
    }
    if (m_pPaceBrush)
    {
        delete m_pPaceBrush;
        m_pPaceBrush = NULL;
    }
    if (m_pMap)
    {
        delete m_pMap;
        m_pMap = NULL;
    }
	GdiplusShutdown(gdiplusToken);
}

void RouteFilter::Run() 
{
    if (!m_pMap) 
        return;

    if (g_VFVAPIVersion >= 12) 
    {
		const VDXPixmap& pxdst = *fa->dst.mpPixmap;
		const VDXPixmap& pxsrc = *fa->src.mpPixmap;

		switch(pxdst.format) {
			case nsVDXPixmap::kPixFormat_XRGB8888:
                Bitmap *pbmp = PrepareRGB32(pxdst.data, pxdst.pitch, pxdst.w, pxdst.h);
                DrawRoute(pbmp, fa->pfsi->lSourceFrameMS);
                ApplyRGB32(pbmp, pxdst.data, pxdst.pitch, pxdst.w, pxdst.h);
                delete pbmp;
				break;
		}
	} 
    else 
    {
        Bitmap *pbmp = PrepareRGB32(fa->dst.data, fa->dst.pitch, fa->dst.w, fa->dst.h);
        DrawRoute(pbmp, fa->pfsi->lSourceFrameMS);
        ApplyRGB32(pbmp, fa->dst.data, fa->dst.pitch, fa->dst.w, fa->dst.h);
        delete pbmp;
	}
}

Bitmap* RouteFilter::PrepareRGB32(void *data, uint32 pitch, uint32 w, uint32 h)
{
    //copy frame to bitmap
    Bitmap *pbmp = new Bitmap(w, h, PixelFormat32bppRGB);
    BitmapData bmpdata;
    Rect rect(0, 0, w, h);
    pbmp->LockBits(&rect, ImageLockModeRead, PixelFormat32bppRGB, &bmpdata);

    uint8 *pbmp8 = (uint8 *)bmpdata.Scan0;
    uint8 *pfrm8 = (uint8 *)data;

    for(uint32 y = 0; y < h; y++)
    {
        memcpy(pbmp8, pfrm8, w*4);
        pfrm8 += pitch;
        pbmp8 += bmpdata.Stride;
    }

    pbmp->UnlockBits(&bmpdata);

    return pbmp;
}

void RouteFilter::ApplyRGB32(Bitmap *pbmp, void *data, uint32 pitch, uint32 w, uint32 h)
{
    BitmapData bmpdata;
    Rect rect(0, 0, w, h);
    pbmp->LockBits(&rect, ImageLockModeRead, PixelFormat32bppARGB, &bmpdata);

    uint8 *pbmp8 = (uint8 *)bmpdata.Scan0;
    uint8 *pfrm8 = (uint8 *)data;

    for(uint32 y = 0; y < h; y++)
    {
        memcpy(pfrm8, pbmp8, w*4);
        pfrm8 += pitch;
        pbmp8 += bmpdata.Stride;
    }

    pbmp->UnlockBits(&bmpdata);
}

bool RouteFilter::Configure(VDXHWND hwnd) {
	RouteFilterDialog dlg(mConfig, fa->ifp);

	return dlg.Show((HWND)hwnd);
}

void RouteFilter::GetSettingString(char *buf, int maxlen) {
	SafePrintf(buf, maxlen, " (\"%s\")", mConfig.mFile);
}

void RouteFilter::GetScriptString(char *buf, int maxlen) {
	char file_c[1024];
	int j = 0;
	for(int i = 0; i < sizeof(mConfig.mFile) && mConfig.mFile[i] != 0 && j < sizeof(file_c); i++, j++)
	{
		if (mConfig.mFile[i] == '\\')
		{
			file_c[j++] = '\\';
		}
		file_c[j] = mConfig.mFile[i]; 
	}
	file_c[j] = 0;

	SafePrintf(buf, maxlen, "Config(\"%s\")", file_c);
}

void RouteFilter::DrawRoute(Bitmap *pbmp, uint32 ms) 
{
    if (!m_pMap) 
        return;

    Graphics graphics(pbmp);

    //draw time text
    int32 time_run = ms-m_TimeOffset;
    if (time_run < 0)
    {
        time_run = 0;
    }
    WCHAR wstr[256];
    uint32 hour = (time_run/1000)/3600;
    uint32 min = ((time_run/1000)%3600)/60;
    uint32 sec = (time_run/1000)%60;
    wsprintfW(wstr, L"�����: %02i:%02i'%02i", hour, min, sec); 
    graphics.DrawString(wstr, wcslen(wstr), m_pTimeFont, m_TimePos, m_pTimeBrush); 

    pugi::xml_node last_sample = m_current_Sample;
    double elapsed_time = m_current_Sample.attribute("elapsedTime").as_double();
    while(time_run > (elapsed_time+1)*1000)
    {
        m_current_Sample = m_current_Sample.next_sibling();
        if (m_current_Sample.empty())
        {
            m_current_Sample = last_sample;
            break;
        }
        elapsed_time = m_current_Sample.attribute("elapsedTime").as_double();
    }
    while(time_run < elapsed_time*1000)
    {
        m_current_Sample = m_current_Sample.previous_sibling();
        if (m_current_Sample.empty())
        {
            m_current_Sample = last_sample;
            break;
        }
        elapsed_time = m_current_Sample.attribute("elapsedTime").as_double();
    }

    int pace_m = m_current_Sample.attribute("pace").as_int();
    if (m_AvgTail > 1)
    {
        pugi::xml_node sample = m_current_Sample.previous_sibling();
        for(int i = 1; i < m_AvgTail; i++)
        {
            if (!sample.empty())
            {
                pace_m += sample.attribute("pace").as_int();
                sample = sample.previous_sibling();
            }
            else
            {
                pace_m += 30*60;
            }
        }
        pace_m /= m_AvgTail;
    }
    int pace_s = pace_m%60;
    pace_m/=60;
    wsprintfW(wstr, L"����: %i'%02i", pace_m, pace_s); 
    graphics.DrawString(wstr, wcslen(wstr), m_pPaceFont, m_PacePos, m_pPaceBrush); 

    int32 last_lap = last_sample.attribute("lapNumber").as_int();
    int32 cur_lap = m_current_Sample.attribute("lapNumber").as_int();
    if (last_lap != cur_lap)
    {
        //rebuild stage image
    }
    int32 image_x = m_current_Sample.attribute("imageX").as_int();
    int32 image_y = m_current_Sample.attribute("imageY").as_int();
    double head_direction = m_current_Sample.attribute("direction").as_double();

    Matrix last_matrix;
    graphics.GetTransform(&last_matrix);

    Region old_clip_region;
    graphics.GetClip(&old_clip_region);
    Rect clip_rect(m_StageMapX, m_StageMapY, m_StageMapWidth, m_StageMapHeight);
    graphics.SetClip(clip_rect);

    Matrix rotate_at_map;
    PointF center(m_StageMapX+m_StageMapWidth/2, m_StageMapY+m_StageMapHeight/2);
    PointF image_pos(image_x, image_y);
    rotate_at_map.Translate(center.X-image_x, center.Y-image_y);
    rotate_at_map.RotateAt(-head_direction, image_pos);
    graphics.SetTransform(&rotate_at_map);

    graphics.DrawImage(m_pMap, 0, 0);
    ColorMatrix ClrMatrix = { 
            1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 0.0f, m_StageMapOpaque/(100.0f), 0.0f,
            0.0f, 0.0f, 0.0f, 0.0f, 1.0f
    };
    ImageAttributes ImgAttr;
    ImgAttr.SetColorMatrix(&ClrMatrix, ColorMatrixFlagsDefault, ColorAdjustTypeBitmap);

    Rect    destination(0, 0, m_pMap->GetWidth(), m_pMap->GetHeight());
    graphics.DrawImage(m_pMap, destination, 0, 0, m_pMap->GetWidth(), m_pMap->GetHeight(), UnitPixel, &ImgAttr);

    //if (m_StageMapOpaque == 100)
    //{
    //    graphics.DrawImage(m_pMap, m_StageMapX, m_StageMapY, m_StageMapWidth, m_StageMapHeight);
    //}
    //else
    //{
    //    ColorMatrix ClrMatrix = { 
    //            1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
    //            0.0f, 1.0f, 0.0f, 0.0f, 0.0f,
    //            0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
    //            0.0f, 0.0f, 0.0f, m_StageMapOpaque/(100.0f), 0.0f,
    //            0.0f, 0.0f, 0.0f, 0.0f, 1.0f
    //    };
    //    ImageAttributes ImgAttr;
    //    ImgAttr.SetColorMatrix(&ClrMatrix, ColorMatrixFlagsDefault, ColorAdjustTypeBitmap);

    //    Rect    destination(m_StageMapX, m_StageMapY, m_StageMapWidth, m_StageMapHeight);
    //    graphics.DrawImage(m_pMap, destination, 0, 0, m_pMap->GetWidth(), m_pMap->GetHeight(), UnitPixel, &ImgAttr);
    //}
    graphics.SetTransform(&last_matrix);
    graphics.SetClip(&old_clip_region);

    return;

}

void RouteFilter::ScriptConfig(IVDXScriptInterpreter *isi, const VDXScriptValue *argv, int argc) {
	strcpy_s(mConfig.mFile, 1024, *argv[0].asString());
}

///////////////////////////////////////////////////////////////////////////////

extern VDXFilterDefinition filterDef_RouteAdd = VDXVideoFilterDefinition<RouteFilter>("Vorfol", "Add Route", "Add route.");
