// /s c:\rgmapvideo\debug.jobs
#include <windows.h>
#include <commctrl.h>
#include <gdiplus.h>
#include <stdio.h>
#include <vector>
#include <memory>
#include <sstream>
#include <regex>

#include <vd2/VDXFrame/VideoFilter.h>
#include <vd2/VDXFrame/VideoFilterDialog.h>
#include "../resource.h"
#include "gdiplusheaders.h"

#include "pugixml.hpp"

#define _USE_MATH_DEFINES
#include "math.h"

using namespace Gdiplus;
#pragma comment (lib,"Gdiplus.lib")

extern int g_VFVAPIVersion;

class GDIToken
{
protected:
    GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR           gdiplusToken;
public:
    GDIToken()
    {
        GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);
    }
    ~GDIToken()
    {
        //GdiplusShutdown(gdiplusToken);
    }
};

///////////////////////////////////////////////////////////////////////////////
interface Holder
{
public:
	virtual std::string ToString() = 0;
	virtual void FromString(const std::string&) = 0;
};

template<class T> class THolder : public Holder
{
protected:
	T & _t;
public:
	explicit THolder(T& t) : _t(t)
	{
		//cout << " of " << typeid(T).name() << endl; 
	}

	std::string ToString()
	{
		std::ostringstream oss;
		oss << _t;
		return oss.str();
	}

	void FromString(const std::string& s)
	{
		std::istringstream iss(s);
		iss >> _t;
	}
};
///////////////////////////////////////////////////////////////////////////////
template<class Key>
class StringStorage
{
public:
	virtual void Load() = 0;
	virtual void Store() = 0;

	std::vector<std::pair<Key, Holder*>> _key_values_pairs;
};

class DLGStorage : public StringStorage<int>
{
protected:
	HWND    _hdlg;
public:
	void SetHDLG(HWND hdlg)
	{
		_hdlg = hdlg;
	}

	void Load()
	{
		for (auto& e : _key_values_pairs)
		{
			try
			{
				HWND h = GetDlgItem(_hdlg, e.first);
				int l = GetWindowTextLength(h);
				char *ptxt = new char[l + 1];
				GetWindowText(h, ptxt, l + 1);
				e.second->FromString(ptxt);
				delete[] ptxt;
			}
			catch (...)
			{
				//TODO: log
			}
		}
	}

	void Store()
	{
		for (auto& e : _key_values_pairs)
		{
			SetDlgItemText(_hdlg, e.first, e.second->ToString().c_str());
		}
	}
};
///////////////////////////////////////////////////////////////////////////////
#pragma region RouteFilterConfig


struct RouteComment
{
public:
	std::string m_Text;
	int			m_Time;	//start in seconds from video start
	int			m_Dur;	//duration
};

class RouteFilterConfig 
{
public:
	RouteFilterConfig()
	{
        InitDef();
		//m_DLGStorage._key_values_pairs.push_back(std::make_pair(IDC_POS_X, std::move(new THolder<int>(m_PosX))));
		//m_DLGStorage._key_values_pairs.push_back(std::make_pair(IDC_POS_Y, std::move(new THolder<int>(m_PosY))));
	}

	//RouteFilterConfig(const RouteFilterConfig &cfg);

    void            InitDef();
    std::string     BuildConfigString();
    bool            BuildXml(pugi::xml_document &doc);

    bool            From_ConfigString(const std::string&);
    bool            FromXml(pugi::xml_document &doc);

public:

	//DLGStorage		m_DLGStorage;

	std::string		m_File;       //save/load config - do not stored in config string

	std::string		m_Map;        //map file
	std::string		m_Logo;       //logo file
	std::string		m_Path;       //path file
	std::string		m_Pointer;    //pointer file

    int             m_PointerOpaque; 

    //current position view
    int             m_PosX; 
    int             m_PosY; 
    int             m_PosWidth; 
    int             m_PosHeight;
    int             m_PosOpaque; 
    int             m_PosTail;
	Gdiplus::Color  m_PosColor;        //with alfa channel
    int             m_PosSize;

    //leg view
    int             m_LegX;
    int             m_LegY;
    int             m_LegWidth;
    int             m_LegHeight;
    int             m_LegOpaque;
    int             m_LegMargins;
	Gdiplus::Color  m_LegColor;        //with alfa channel
    int             m_LegSize;

    //logo view
    int             m_LogoX;
    int             m_LogoY;
    int             m_LogoOpaque;

    int32           m_PathOffset;   //in sec from start of video (may be negative)
    
    //text view
    int             m_TextX;
    int             m_TextY;
    int             m_TextWidth;
    int             m_TextHeight;
    Color           m_TextColor;    //with alfa channel

    //run time (inside text view)
    int             m_TimeX;
    int             m_TimeY;
	std::string		m_TimeFont;
    int             m_TimeSize;
	Gdiplus::Color  m_TimeColor;
   	
    //pace (inside text view)
    int             m_PaceX;
    int             m_PaceY;
	std::string		m_PaceFont;
    int             m_PaceSize;
	Gdiplus::Color  m_PaceColor;
    int             m_PaceAvg;    //in samples - to calculate average pace

    //pulse (inside text view)
    int             m_PulseX;
    int             m_PulseY;
	std::string		m_PulseFont;
    int             m_PulseSize;
	Gdiplus::Color  m_PulseColor;

	//comments
	std::vector<RouteComment> m_Comments;
	int				m_CommX;
	int				m_CommY;
	int				m_CommW;
	int				m_CommH;
	std::string		m_CommFont;
	int             m_CommSize;
	Color			m_CommColor;
};

void RouteFilterConfig::InitDef()
{
    m_File = "settings.xml";
    m_Map = "map.jpg";
    m_Logo = "logo.png";
    m_Path = "route.xml";
    m_Pointer = "pointer.png";

    m_PointerOpaque = 80; 

    //current position view
    m_PosX = 960; 
    m_PosY = 400; 
    m_PosWidth = 300; 
    m_PosHeight = 300;
    m_PosOpaque = 80; 
    m_PosTail = 60;
    m_PosColor = Gdiplus::Color(128, 255, 0, 0);
    m_PosSize = 3;

    //leg view
    m_LegX = 20;
    m_LegY = 200;
    m_LegWidth = 300;
    m_LegHeight = 500;
    m_LegOpaque = 90;
    m_LegMargins = 30;
    m_LegColor = Gdiplus::Color(128, 255, 0, 0);
    m_LegSize = 3;

    //logo view
    m_LogoX = 1132;
    m_LogoY = 20;
    m_LogoOpaque = 80;

    m_PathOffset = 0;   //in sec from start of video (may be negative)
    
    //text view
    m_TextX = 0;
    m_TextY = 0;
    m_TextWidth = 200;
    m_TextHeight = 80;
    m_TextColor = Color(128,80,80,80);    //with alfa channel

    //run time (inside text view)
    m_TimeX = 0;
    m_TimeY = 0;
    m_TimeFont = "Courier";
    m_TimeSize = 16;
    m_TimeColor = Color(255,255,255,255);
   	
    //pace (inside text view)
    m_PaceX = 0;
    m_PaceY = 20;
    m_PaceFont = "Courier";
    m_PaceSize = 16;
    m_PaceColor = Color(255,255,255,255);
    m_PaceAvg = 30;  //in samples - to calculate average pace

    //pulse (inside text view)
    m_PulseX = 0;
    m_PulseY = 40;
    m_PulseFont = "Courier";
    m_PulseSize = 16;
    m_PulseColor = Color(255,255,255,255);

	m_CommX = 300;
	m_CommY = 600;
	m_CommW = 600;
	m_CommH = 100;
	m_CommFont = "Courier";
	m_CommSize = 16;
	m_CommColor = Color(255, 255, 255, 255);

	m_Comments.clear();

}

std::ostream& operator << (std::ostream& o, const Color& c)
{
	o << std::hex << c.GetValue();
	return o;
}

std::istream& operator >> (std::istream& i, Color& c)
{
	unsigned long v;
	i >> std::hex >> v;
	c.SetValue(v);
	return i;
}

//std::string hexStringFromColor(Color& c)
//{
//	std::ostringstream o;
//	o << c;
//	return o.str();
//}
//
//Color colorFromHexString(std::string s)
//{
//	Color c;
//	std::istringstream i(s);
//	i >> c;
//	return c;
//}

template<class T>
std::string stringFrom(const T& t)
{
	std::ostringstream o;
	o << t;
	return o.str();
}

template<>
std::string stringFrom<std::string>(const std::string& t)
{
	return t;
}

template<class T>
T fromString(const std::string& s)
{
	T t;
	std::istringstream i(s);
	i >> t;
	return t;
}

template<>
std::string fromString(const std::string& s)
{
	return s;
}

bool  RouteFilterConfig::BuildXml(pugi::xml_document &doc)
{
    doc.reset();
    pugi::xml_node settings = doc.append_child("RouteAddSettings");

    pugi::xml_node map_file = settings.append_child("Map");
    map_file.text().set(m_Map.c_str());

    pugi::xml_node logo_file = settings.append_child("Logo");
    logo_file.text().set(m_Logo.c_str());
    logo_file.append_attribute("x") = m_LogoX;
    logo_file.append_attribute("y") = m_LogoY;
    logo_file.append_attribute("opaq") = m_LogoOpaque;

    pugi::xml_node pointer_file = settings.append_child("Pointer");
    pointer_file.text().set(m_Pointer.c_str());
    pointer_file.append_attribute("opaq") = m_PointerOpaque;

    pugi::xml_node path_file = settings.append_child("Path");
    path_file.text().set(m_Path.c_str());
    path_file.append_attribute("offset") = m_PathOffset;

    pugi::xml_node leg_view = settings.append_child("Leg");
    leg_view.append_attribute("x") = m_LegX;
    leg_view.append_attribute("y") = m_LegY;
    leg_view.append_attribute("w") = m_LegWidth;
    leg_view.append_attribute("h") = m_LegHeight;
    leg_view.append_attribute("opaq") = m_LegOpaque;
    leg_view.append_attribute("color") = stringFrom(m_LegColor).c_str();
    leg_view.append_attribute("size") = m_LegSize;
    leg_view.append_attribute("margins") = m_LegMargins;

    pugi::xml_node pos_view = settings.append_child("Pos");
    pos_view.append_attribute("x") = m_PosX;
    pos_view.append_attribute("y") = m_PosY;
    pos_view.append_attribute("w") = m_PosWidth;
    pos_view.append_attribute("h") = m_PosHeight;
    pos_view.append_attribute("opaq") = m_PosOpaque;
    pos_view.append_attribute("color") = stringFrom(m_PosColor).c_str();
    pos_view.append_attribute("size") = m_PosSize;

    pugi::xml_node text_view = settings.append_child("Text");
    text_view.append_attribute("x") = m_TextX;
    text_view.append_attribute("y") = m_TextY;
    text_view.append_attribute("w") = m_TextWidth;
    text_view.append_attribute("h") = m_TextHeight;
    text_view.append_attribute("color") = stringFrom(m_TextColor).c_str();

    pugi::xml_node time_text = settings.append_child("Time");
    time_text.append_attribute("x") = m_TimeX;
    time_text.append_attribute("y") = m_TimeY;
    time_text.append_attribute("size") = m_TimeSize;
    time_text.append_attribute("font") = m_TimeFont.c_str();
    time_text.append_attribute("color") = stringFrom(m_TimeColor).c_str();

    pugi::xml_node pace_text = settings.append_child("Pace");
    pace_text.append_attribute("x") = m_PaceX;
    pace_text.append_attribute("y") = m_PaceY;
    pace_text.append_attribute("size") = m_PaceSize;
    pace_text.append_attribute("font") = m_PaceFont.c_str();
    pace_text.append_attribute("color") = stringFrom(m_PaceColor).c_str();

	pugi::xml_node pulse_text = settings.append_child("Pulse");
	pulse_text.append_attribute("x") = m_PulseX;
	pulse_text.append_attribute("y") = m_PulseY;
	pulse_text.append_attribute("size") = m_PulseSize;
	pulse_text.append_attribute("font") = m_PulseFont.c_str();
	pulse_text.append_attribute("color") = stringFrom(m_PulseColor).c_str();

	pugi::xml_node Comm_text = settings.append_child("Comm");
	Comm_text.append_attribute("x") = m_CommX;
	Comm_text.append_attribute("y") = m_CommY;
	Comm_text.append_attribute("w") = m_CommW;
	Comm_text.append_attribute("h") = m_CommH;
	Comm_text.append_attribute("size") = m_CommSize;
	Comm_text.append_attribute("font") = m_CommFont.c_str();
	Comm_text.append_attribute("color") = stringFrom(m_CommColor).c_str();

	for (int i = 0; i < m_Comments.size(); i++)
	{
		pugi::xml_node Comment_text = Comm_text.append_child("Text");
		Comment_text.text() = m_Comments[i].m_Text.c_str();
		Comment_text.append_attribute("time") = m_Comments[i].m_Time;
		Comment_text.append_attribute("dur") = m_Comments[i].m_Dur;
	}

	return true;
}

bool RouteFilterConfig::FromXml(pugi::xml_document &doc)
{
    pugi::xml_node settings = doc.child("RouteAddSettings");

    pugi::xml_node map_file = settings.child("Map");
    m_Map = map_file.child_value();

    pugi::xml_node logo_file = settings.child("Logo");
    m_Logo = logo_file.child_value();
    m_LogoX = logo_file.attribute("x").as_int();
    m_LogoY = logo_file.attribute("y").as_int();
    m_LogoOpaque = logo_file.attribute("opaq").as_int();

    pugi::xml_node pointer_file = settings.child("Pointer");
    m_Pointer = pointer_file.child_value();
    m_PointerOpaque = pointer_file.attribute("opaq").as_int();

    pugi::xml_node path_file = settings.child("Path");
    m_Path = path_file.child_value();
    m_PathOffset = path_file.attribute("offset").as_int();

    pugi::xml_node leg_view = settings.child("Leg");
    m_LegX = leg_view.attribute("x").as_int();
    m_LegY = leg_view.attribute("y").as_int();
    m_LegWidth = leg_view.attribute("w").as_int();
    m_LegHeight = leg_view.attribute("h").as_int();
    m_LegOpaque = leg_view.attribute("opaq").as_int();
    m_LegColor = fromString<Color>(leg_view.attribute("color").as_string());
    m_LegSize = leg_view.attribute("size").as_int();
    m_LegMargins = leg_view.attribute("margins").as_int();

    pugi::xml_node pos_view = settings.child("Pos");
    m_PosX = pos_view.attribute("x").as_int();
    m_PosY = pos_view.attribute("y").as_int();
    m_PosWidth = pos_view.attribute("w").as_int();
    m_PosHeight = pos_view.attribute("h").as_int();
    m_PosOpaque = pos_view.attribute("opaq").as_int();
    m_PosColor = fromString<Color>(pos_view.attribute("color").as_string());
    m_PosSize = pos_view.attribute("size").as_int();

    pugi::xml_node text_view = settings.child("Text");
    m_TextX = text_view.attribute("x").as_int();
    m_TextY = text_view.attribute("y").as_int();
    m_TextWidth = text_view.attribute("w").as_int();
    m_TextHeight = text_view.attribute("h").as_int();
    m_TextColor = fromString<Color>(text_view.attribute("color").as_string());

    pugi::xml_node time_text = settings.child("Time");
    m_TimeX = time_text.attribute("x").as_int();
    m_TimeY = time_text.attribute("y").as_int();
    m_TimeFont = time_text.attribute("font").as_string();
    m_TimeColor = fromString<Color>(time_text.attribute("color").as_string());
    m_TimeSize = time_text.attribute("size").as_int();

    pugi::xml_node pace_text = settings.child("Pace");
    m_PaceX = pace_text.attribute("x").as_int();
    m_PaceY = pace_text.attribute("y").as_int();
    m_PaceFont = pace_text.attribute("font").as_string();
    m_PaceColor = fromString<Color>(pace_text.attribute("color").as_string());
    m_PaceSize = pace_text.attribute("size").as_int();

	pugi::xml_node pulse_text = settings.child("Pulse");
	m_PulseX = pulse_text.attribute("x").as_int();
	m_PulseY = pulse_text.attribute("y").as_int();
	m_PulseFont = pulse_text.attribute("font").as_string();
	m_PulseColor = fromString<Color>(pulse_text.attribute("color").as_string());
	m_PulseSize = pulse_text.attribute("size").as_int();

	pugi::xml_node Comm_text = settings.child("Comm");
	m_CommX = Comm_text.attribute("x").as_int();
	m_CommY = Comm_text.attribute("y").as_int();
	m_CommW = Comm_text.attribute("w").as_int();
	m_CommH = Comm_text.attribute("h").as_int();
	m_CommFont = Comm_text.attribute("font").as_string();
	m_CommColor = fromString<Color>(Comm_text.attribute("color").as_string());
	m_CommSize = Comm_text.attribute("size").as_int();

	m_Comments.clear();
	pugi::xml_node Comment_text = Comm_text.child("Text");
	while (!Comment_text.empty())
	{
		RouteComment comment;
		comment.m_Text = Comment_text.child_value();
		comment.m_Time = Comment_text.attribute("time").as_int();
		comment.m_Dur = Comment_text.attribute("dur").as_int();
		m_Comments.push_back(comment);
		Comment_text = Comment_text.next_sibling();
	}

	return true;
}

std::string base64_encode(unsigned char const* bytes_to_encode, unsigned int in_len);
std::string base64_decode(std::string const& encoded_string);

std::string RouteFilterConfig::BuildConfigString()
{
    pugi::xml_document doc;
    BuildXml(doc);

    std::stringstream doc_stream;
    doc.save(doc_stream);

    std::string encoded = base64_encode((const unsigned char*)(doc_stream.str().c_str()), doc_stream.str().size());

    return encoded;
}

bool RouteFilterConfig::From_ConfigString(const std::string& config_str)
{
    std::string decoded = base64_decode(config_str);

    std::stringstream doc_stream(decoded);
    
    pugi::xml_document doc;
    if (doc.load(doc_stream))
    {
        return FromXml(doc);
    }
    return false;
}
#pragma endregion RouteFilterConfig


#pragma region ConfigDialog
///////////////////////////////////////////////////////////////////////////////


class RouteFilterDialog : public VDXVideoFilterDialog {
public:
	RouteFilterDialog(RouteFilterConfig& config, IVDXFilterPreview *ifp) : m_Config(config) 
    {
    }

	bool Show(HWND parent) 
    {
		return 0 != VDXVideoFilterDialog::Show(NULL, MAKEINTRESOURCE(IDD_FILTER_ROUTE_DLG), parent);
	}

	virtual INT_PTR DlgProc(UINT msg, WPARAM wParam, LPARAM lParam);

protected:

	bool OnInit();
	bool OnCommand(int cmd);
	void OnDestroy();

	void LoadFromConfig();
	bool SaveToConfig();

    bool DoBrowse(const char* filter, std::string& file);

protected:

	RouteFilterConfig& m_Config;
	RouteFilterConfig m_OldConfig;
};

template<class T>
bool SetDLGFromVal(HWND hdlg, int idc, const T& v);

template<>
bool SetDLGFromVal<int>(HWND hdlg, int idc, const int& v)
{
	return SetDlgItemInt(hdlg, idc, v, TRUE) != FALSE;
}

template<>
bool SetDLGFromVal<std::string>(HWND hdlg, int idc, const std::string& v)
{
	return SetDlgItemText(hdlg, idc, v.c_str()) != FALSE;
}

template<class T>
bool SetDLGFromVal(HWND hdlg, int idc, const T& v)
{
	return SetDlgItemText(hdlg, idc, stringFrom(v).c_str()) != FALSE;
}

template<class T>
bool GetValFromDLG(HWND hdlg, int idc, T& v)
{
	HWND h = GetDlgItem(hdlg, idc);
	int l = GetWindowTextLength(h);
	char *ptxt = new char[l + 1];
	GetWindowText(h, ptxt, l + 1);
	v = fromString<T>(ptxt);
	delete[] ptxt;
	return true;
}

template<>
bool GetValFromDLG<int>(HWND hdlg, int idc, int& v)
{
	BOOL tr;
	v = (int)GetDlgItemInt(hdlg, idc, &tr, TRUE);
	return tr != FALSE;
}

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

bool RouteFilterDialog::OnInit() 
{
	m_OldConfig = m_Config;

	LoadFromConfig();

	return false;
}

bool RouteFilterDialog::DoBrowse(const char* filter, std::string &file)
{
	OPENFILENAME ofn; 
	char file_buf[512];
	strcpy_s(file_buf, file.c_str());
    // Initialize OPENFILENAME
	ZeroMemory(&ofn, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = mhdlg;
	ofn.lpstrFile = file_buf;
	ofn.nMaxFile = _countof(file_buf);
	ofn.lpstrFilter = filter;
	ofn.nFilterIndex = 1;
	ofn.lpstrFileTitle = NULL;
	ofn.nMaxFileTitle = 0;
	ofn.lpstrInitialDir = NULL;
	ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

	// Display the Open dialog box. 

	if (GetOpenFileName(&ofn) != FALSE)
	{
		file = file_buf;
		return true;
	}
	return false;
}


void RouteFilterDialog::OnDestroy() 
{
}

bool RouteFilterDialog::OnCommand(int cmd) {
	switch(cmd) {
		case IDOK:
			SaveToConfig();
			EndDialog(mhdlg, TRUE);
			return true;

		case IDCANCEL:
			m_Config = m_OldConfig;
			EndDialog(mhdlg, FALSE);
			return true;

        case IDC_SAVE:
            {
                SaveToConfig();
                pugi::xml_document doc;
                m_Config.BuildXml(doc);
                doc.save_file(m_Config.m_File.c_str());
            }
            return true;

		case IDC_BROWSE:
            if (DoBrowse("All\0*.*\0XML\0*.xml\0", m_Config.m_File))
            {
				SetDLGFromVal(mhdlg, IDC_FILE, m_Config.m_File);
                pugi::xml_document doc;
                pugi::xml_parse_result res = doc.load_file(m_Config.m_File.c_str());
                if (res)
                {
                    m_Config.FromXml(doc);
                    LoadFromConfig();
				}
			}
			return true;

        case IDC_BROWSE_MAP:
            if (DoBrowse("All\0*.*\0", m_Config.m_Map))
            {
				SetDLGFromVal(mhdlg, IDC_FILE_MAP, m_Config.m_Map);
			}
			return true;

        case IDC_BROWSE_PATH:
            if (DoBrowse("All\0*.*\0", m_Config.m_Path))
            {
				SetDLGFromVal(mhdlg, IDC_FILE_PATH, m_Config.m_Path);
			}
			return true;

        case IDC_BROWSE_POINTER:
            if (DoBrowse("All\0*.*\0", m_Config.m_Pointer))
            {
				SetDLGFromVal(mhdlg, IDC_FILE_POINTER, m_Config.m_Pointer);
			}
			return true;

        case IDC_BROWSE_LOGO:
            if (DoBrowse("All\0*.*\0", m_Config.m_Logo))
            {
				SetDLGFromVal(mhdlg, IDC_FILE_LOGO, m_Config.m_Logo);
			}
			return true;
}

	return false;
}

void RouteFilterDialog::LoadFromConfig()
{
	SetDLGFromVal(mhdlg, IDC_FILE, m_Config.m_File);
	SetDLGFromVal(mhdlg, IDC_FILE_MAP, m_Config.m_Map);
	SetDLGFromVal(mhdlg, IDC_FILE_LOGO, m_Config.m_Logo);
    SetDLGFromVal(mhdlg, IDC_FILE_PATH, m_Config.m_Path);
    SetDLGFromVal(mhdlg, IDC_FILE_POINTER, m_Config.m_Pointer);

    SetDLGFromVal(mhdlg, IDC_POINTER_OP, m_Config.m_PointerOpaque);
    
    SetDLGFromVal(mhdlg, IDC_POS_X, m_Config.m_PosX);
    SetDLGFromVal(mhdlg, IDC_POS_Y, m_Config.m_PosY);
    SetDLGFromVal(mhdlg, IDC_POS_W, m_Config.m_PosWidth);
    SetDLGFromVal(mhdlg, IDC_POS_H, m_Config.m_PosHeight);
    SetDLGFromVal(mhdlg, IDC_POS_OP, m_Config.m_PosOpaque);
    SetDLGFromVal(mhdlg, IDC_POS_TAIL, m_Config.m_PosTail);
    SetDLGFromVal(mhdlg, IDC_POS_COLOR, m_Config.m_PosColor);

    SetDLGFromVal(mhdlg, IDC_LEG_X, m_Config.m_LegX);
    SetDLGFromVal(mhdlg, IDC_LEG_Y, m_Config.m_LegY);
    SetDLGFromVal(mhdlg, IDC_LEG_W, m_Config.m_LegWidth);
    SetDLGFromVal(mhdlg, IDC_LEG_H, m_Config.m_LegHeight);
    SetDLGFromVal(mhdlg, IDC_LEG_OP, m_Config.m_LegOpaque);
    SetDLGFromVal(mhdlg, IDC_LEG_MARG, m_Config.m_LegMargins);
    SetDLGFromVal(mhdlg, IDC_LEG_COLOR, m_Config.m_LegColor);

    SetDLGFromVal(mhdlg, IDC_LOGO_X, m_Config.m_LogoX);
    SetDLGFromVal(mhdlg, IDC_LOGO_Y, m_Config.m_LogoY);
    SetDLGFromVal(mhdlg, IDC_LOGO_OP, m_Config.m_LogoOpaque);

    SetDLGFromVal(mhdlg, IDC_TEXT_X, m_Config.m_TextX);
    SetDLGFromVal(mhdlg, IDC_TEXT_Y, m_Config.m_TextY);
    SetDLGFromVal(mhdlg, IDC_TEXT_W, m_Config.m_TextWidth);
    SetDLGFromVal(mhdlg, IDC_TEXT_H, m_Config.m_TextHeight);
    SetDLGFromVal(mhdlg, IDC_TEXT_COLOR, m_Config.m_TextColor);

    SetDLGFromVal(mhdlg, IDC_TIME_X, m_Config.m_TimeX);
    SetDLGFromVal(mhdlg, IDC_TIME_Y, m_Config.m_TimeY);
    SetDLGFromVal(mhdlg, IDC_TIME_FONT, m_Config.m_TimeFont);
    SetDLGFromVal(mhdlg, IDC_TIME_SIZE, m_Config.m_TimeSize);
    SetDLGFromVal(mhdlg, IDC_TIME_COLOR, m_Config.m_TimeColor);

    SetDLGFromVal(mhdlg, IDC_PACE_X, m_Config.m_PaceX);
    SetDLGFromVal(mhdlg, IDC_PACE_Y, m_Config.m_PaceY);
    SetDLGFromVal(mhdlg, IDC_PACE_FONT, m_Config.m_PaceFont);
    SetDLGFromVal(mhdlg, IDC_PACE_SIZE, m_Config.m_PaceSize);
    SetDLGFromVal(mhdlg, IDC_PACE_COLOR, m_Config.m_PaceColor);
    SetDLGFromVal(mhdlg, IDC_PACE_AVG, m_Config.m_PaceAvg);

    SetDLGFromVal(mhdlg, IDC_PULSE_X, m_Config.m_PulseX);
    SetDLGFromVal(mhdlg, IDC_PULSE_Y, m_Config.m_PulseY);
    SetDLGFromVal(mhdlg, IDC_PULSE_FONT, m_Config.m_PulseFont);
    SetDLGFromVal(mhdlg, IDC_PULSE_SIZE, m_Config.m_PulseSize);
    SetDLGFromVal(mhdlg, IDC_PULSE_COLOR, m_Config.m_PulseColor);

	SetDLGFromVal(mhdlg, IDC_COMM_X, m_Config.m_CommX);
	SetDLGFromVal(mhdlg, IDC_COMM_Y, m_Config.m_CommY);
	SetDLGFromVal(mhdlg, IDC_COMM_W, m_Config.m_CommW);
	SetDLGFromVal(mhdlg, IDC_COMM_H, m_Config.m_CommH);
	SetDLGFromVal(mhdlg, IDC_COMM_FONT, m_Config.m_CommFont);
	SetDLGFromVal(mhdlg, IDC_COMM_SIZE, m_Config.m_CommSize);
	SetDLGFromVal(mhdlg, IDC_COMM_COLOR, m_Config.m_CommColor);
	
	//std::stringstream str_comm;
	//for (int i = 0; i < m_Config.m_Comments.size(); i++)
	//{
	//	str_comm.fill('0');
	//	str_comm.width(2);
	//	str_comm << m_Config.m_Comments[i].m_Time / 3600;
	//	str_comm << ":";
	//	str_comm << (m_Config.m_Comments[i].m_Time % 3600) / 60;
	//	str_comm << "'";
	//	str_comm << (m_Config.m_Comments[i].m_Time % 3600) % 60;
	//	str_comm << ",";
	//	str_comm << m_Config.m_Comments[i].m_Dur;
	//	str_comm << ",";
	//	str_comm << m_Config.m_Comments[i].m_Text;
	//	str_comm << "\r\n";
	//}
	//SetDlgItemText(mhdlg, IDC_COMMENTS, str_comm.str().c_str());

    SetDLGFromVal(mhdlg, IDC_PATH_OFFSET, m_Config.m_PathOffset);

}

bool RouteFilterDialog::SaveToConfig()
{
	GetValFromDLG(mhdlg, IDC_FILE, m_Config.m_File);
	GetValFromDLG(mhdlg, IDC_FILE_MAP, m_Config.m_Map);
	GetValFromDLG(mhdlg, IDC_FILE_LOGO, m_Config.m_Logo);
	GetValFromDLG(mhdlg, IDC_FILE_PATH, m_Config.m_Path);
	GetValFromDLG(mhdlg, IDC_FILE_POINTER, m_Config.m_Pointer);

	GetValFromDLG(mhdlg, IDC_POINTER_OP, m_Config.m_PointerOpaque);

	GetValFromDLG(mhdlg, IDC_POS_X, m_Config.m_PosX);
	GetValFromDLG(mhdlg, IDC_POS_Y, m_Config.m_PosY);
	GetValFromDLG(mhdlg, IDC_POS_W, m_Config.m_PosWidth);
	GetValFromDLG(mhdlg, IDC_POS_H, m_Config.m_PosHeight);
	GetValFromDLG(mhdlg, IDC_POS_OP, m_Config.m_PosOpaque);
	GetValFromDLG(mhdlg, IDC_POS_TAIL, m_Config.m_PosTail);
	GetValFromDLG(mhdlg, IDC_POS_COLOR, m_Config.m_PosColor);

	GetValFromDLG(mhdlg, IDC_LEG_X, m_Config.m_LegX);
	GetValFromDLG(mhdlg, IDC_LEG_Y, m_Config.m_LegY);
	GetValFromDLG(mhdlg, IDC_LEG_W, m_Config.m_LegWidth);
	GetValFromDLG(mhdlg, IDC_LEG_H, m_Config.m_LegHeight);
	GetValFromDLG(mhdlg, IDC_LEG_OP, m_Config.m_LegOpaque);
	GetValFromDLG(mhdlg, IDC_LEG_MARG, m_Config.m_LegMargins);
	GetValFromDLG(mhdlg, IDC_LEG_COLOR, m_Config.m_LegColor);

	GetValFromDLG(mhdlg, IDC_LOGO_X, m_Config.m_LogoX);
	GetValFromDLG(mhdlg, IDC_LOGO_Y, m_Config.m_LogoY);
	GetValFromDLG(mhdlg, IDC_LOGO_OP, m_Config.m_LogoOpaque);

	GetValFromDLG(mhdlg, IDC_TEXT_X, m_Config.m_TextX);
	GetValFromDLG(mhdlg, IDC_TEXT_Y, m_Config.m_TextY);
	GetValFromDLG(mhdlg, IDC_TEXT_W, m_Config.m_TextWidth);
	GetValFromDLG(mhdlg, IDC_TEXT_H, m_Config.m_TextHeight);
	GetValFromDLG(mhdlg, IDC_TEXT_COLOR, m_Config.m_TextColor);

	GetValFromDLG(mhdlg, IDC_TIME_X, m_Config.m_TimeX);
	GetValFromDLG(mhdlg, IDC_TIME_Y, m_Config.m_TimeY);
	GetValFromDLG(mhdlg, IDC_TIME_FONT, m_Config.m_TimeFont);
	GetValFromDLG(mhdlg, IDC_TIME_SIZE, m_Config.m_TimeSize);
	GetValFromDLG(mhdlg, IDC_TIME_COLOR, m_Config.m_TimeColor);

	GetValFromDLG(mhdlg, IDC_PACE_X, m_Config.m_PaceX);
	GetValFromDLG(mhdlg, IDC_PACE_Y, m_Config.m_PaceY);
	GetValFromDLG(mhdlg, IDC_PACE_FONT, m_Config.m_PaceFont);
	GetValFromDLG(mhdlg, IDC_PACE_SIZE, m_Config.m_PaceSize);
	GetValFromDLG(mhdlg, IDC_PACE_COLOR, m_Config.m_PaceColor);
	GetValFromDLG(mhdlg, IDC_PACE_AVG, m_Config.m_PaceAvg);

	GetValFromDLG(mhdlg, IDC_PULSE_X, m_Config.m_PulseX);
	GetValFromDLG(mhdlg, IDC_PULSE_Y, m_Config.m_PulseY);
	GetValFromDLG(mhdlg, IDC_PULSE_FONT, m_Config.m_PulseFont);
	GetValFromDLG(mhdlg, IDC_PULSE_SIZE, m_Config.m_PulseSize);
	GetValFromDLG(mhdlg, IDC_PULSE_COLOR, m_Config.m_PulseColor);

	GetValFromDLG(mhdlg, IDC_COMM_X, m_Config.m_CommX);
	GetValFromDLG(mhdlg, IDC_COMM_Y, m_Config.m_CommY);
	GetValFromDLG(mhdlg, IDC_COMM_W, m_Config.m_CommW);
	GetValFromDLG(mhdlg, IDC_COMM_H, m_Config.m_CommH);
	GetValFromDLG(mhdlg, IDC_COMM_FONT, m_Config.m_CommFont);
	GetValFromDLG(mhdlg, IDC_COMM_SIZE, m_Config.m_CommSize);
	GetValFromDLG(mhdlg, IDC_COMM_COLOR, m_Config.m_CommColor);

	//m_Config.m_Comments.clear();

	//try
	//{

	//	HWND hEdit = GetDlgItem(mhdlg, IDC_COMMENTS);
	//	int comments_len = GetWindowTextLength(hEdit);
	//	if (comments_len > 1)
	//	{
	//		static const char *trim_str = " \t\r\n";
	//		char *comments = new char[comments_len + 1];
	//		GetWindowText(hEdit, comments, comments_len);
	//		std::string str_comments(comments);
	//		int count = 0;
	//		do
	//		{
	//			// trim trailing spaces
	//			size_t endpos = str_comments.find_last_not_of(trim_str);
	//			size_t startpos = str_comments.find_first_not_of(trim_str);
	//			if (std::string::npos != endpos)
	//			{
	//				str_comments = str_comments.substr(0, endpos + 1);
	//			}
	//			if (std::string::npos != startpos)
	//			{
	//				str_comments = str_comments.substr(startpos);
	//			}
	//			count = str_comments.find_first_of("\r\n");
	//			std::string str_line(str_comments, 0, count);
	//			int h, m, s, d;
	//			sscanf(str_line.c_str(), "%i:%i'%i,%i", &h, &m, &s, &d);
	//			RouteComment route_comment;
	//			route_comment.m_Time = h * 3600 + m * 60 + s;
	//			route_comment.m_Dur = d;
	//			route_comment.m_Text = str_line.substr(str_line.find_first_of(',')+1);
	//			route_comment.m_Text = route_comment.m_Text.substr(route_comment.m_Text.find_first_of(',')+1);
	//			m_Config.m_Comments.push_back(route_comment);
	//			if (count != std::string::npos)
	//			{
	//				str_comments = str_comments.substr(count);
	//			}
	//		} while (count != std::string::npos);
	//		delete comments;
	//	}
	//}
	//catch (...)
	//{
	//}

	GetValFromDLG(mhdlg, IDC_PATH_OFFSET, m_Config.m_PathOffset);

	return true;
}

#pragma endregion ConfigDialog

///////////////////////////////////////////////////////////////////////////////

class RouteFilter : public VDXVideoFilter {
public:
    RouteFilter() 
    {
        ZeroVars();
    }

	RouteFilter(const RouteFilter &f)
	{
		ZeroVars();
		m_Config = f.m_Config;
	}

	RouteFilter& operator = (const RouteFilter &f)
	{
		ZeroVars();
		m_Config = f.m_Config;
		return *this;
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

    GDIToken    m_GDIToken;

    virtual void ZeroVars();    //pointers only
    virtual void DeleteVars();
    virtual void CreateVars();  //from m_Config!

    Bitmap *PrepareRGB32(void* data, uint32 pitch, uint32 w, uint32 h);
	void DrawRoute(Bitmap *bmp, uint32 ms);
    void ApplyRGB32(Bitmap *bmp, void* data, uint32 pitch, uint32 w, uint32 h);

	void ScriptConfig(IVDXScriptInterpreter *isi, const VDXScriptValue *argv, int argc);

	RouteFilterConfig	m_Config;

    Bitmap             *m_pMap;

    Bitmap             *m_pPointer;
    Pen                *m_pPenTail;

    Matrix             *m_pLegMatrix;
    std::vector<PointF> m_LegPoints;
    std::vector<double> m_LegTimes;
    Pen                *m_pPenLeg;

    Bitmap             *m_pLogo;

    SolidBrush         *m_pTextBrush;

    Font               *m_pTimeFont;
    SolidBrush         *m_pTimeBrush;
   	
    Font               *m_pPaceFont;
    SolidBrush         *m_pPaceBrush;

    Font               *m_pPulseFont;
    SolidBrush         *m_pPulseBrush;

	Font               *m_pCommFont;
	SolidBrush         *m_pCommBrush;
	
	pugi::xml_document *m_pPathDoc;

    pugi::xml_node      m_LastSample;
    //Bitmap             *m_pLastBmp;

    int                 m_LegPosition;

    bool                m_Initialized;
};

VDXVF_BEGIN_SCRIPT_METHODS(RouteFilter)
VDXVF_DEFINE_SCRIPT_METHOD(RouteFilter, ScriptConfig, "s")
VDXVF_END_SCRIPT_METHODS()

void RouteFilter::ZeroVars()
{
    //m_pLastBmp = NULL;
    m_pPathDoc = NULL;
    m_pMap = NULL; 
    m_pLogo = NULL; 
    m_pPointer = NULL;
    m_pPenTail = NULL;
    m_pPenLeg = NULL;
    m_pTimeBrush = NULL; 
    m_pTextBrush = NULL;
    m_pTimeFont = NULL;
    m_pPaceBrush = NULL; 
    m_pPaceFont = NULL;
    m_pPulseBrush = NULL; 
    m_pPulseFont = NULL;
	m_pCommBrush = NULL;
	m_pCommFont = NULL;
	m_pLegMatrix = NULL;

    m_Initialized = false;
}

void RouteFilter::DeleteVars()
{
    //delete m_pLastBmp;
    delete m_pPathDoc;
    delete m_pMap;
    delete m_pLogo;
    delete m_pPointer;
    delete m_pPenTail;
    delete m_pPenLeg;
    delete m_pTimeBrush;
    delete m_pTextBrush;
    delete m_pTimeFont;
    delete m_pPaceBrush;
    delete m_pPaceFont;
	delete m_pPulseBrush;
	delete m_pPulseFont;
	delete m_pCommBrush;
	delete m_pCommFont;
	delete m_pLegMatrix;
    ZeroVars();
}

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
    DeleteVars();
}

bool RouteFilter::Init()
{
    bool ret = VDXVideoFilter::Init();

    DeleteVars();

    return ret;
}

#define CapsCharToVal(x) (*(x)>'9'?*(x)-'A'+10:*(x)-'0')
#define CapsCharToVal2(x) (CapsCharToVal(x+1)|(CapsCharToVal(x)<<4))

Color ColorFromStr(const char* str)
{
    if (str == NULL || *str == '0') return Color(0,0,0,0);
    BYTE a = CapsCharToVal2(str);
    BYTE r = CapsCharToVal2(str+2);
    BYTE g = CapsCharToVal2(str+4);
    BYTE b = CapsCharToVal2(str+6);
    return Color(a,r,g,b);
}

void RouteFilter::CreateVars()
{
    if (m_Config.m_File[0] != 0)
    {
        //load map
        m_pMap = Bitmap::FromFile(pugi::as_wide(m_Config.m_Map).c_str());
        //load path
        m_pPathDoc = new pugi::xml_document();
        pugi::xml_parse_result result = m_pPathDoc->load_file(m_Config.m_Path.c_str());
        pugi::xml_node empty_node;
        m_LastSample = empty_node;// m_pPathDoc->child("Route").child("Segment").first_child();
        //load logo
        m_pLogo = Bitmap::FromFile(pugi::as_wide(m_Config.m_Logo).c_str());
        //load pointer
        m_pPointer = Bitmap::FromFile(pugi::as_wide(m_Config.m_Pointer).c_str());
        //create some resourses
        m_pTimeFont = new Font(pugi::as_wide(m_Config.m_TimeFont).c_str(), m_Config.m_TimeSize);
        m_pTimeBrush = new SolidBrush(m_Config.m_TimeColor); 
        m_pTextBrush = new SolidBrush(m_Config.m_TextColor); 
        m_pPenTail = new Pen(m_Config.m_PosColor, m_Config.m_PosSize);
        m_pPenLeg = new Pen(m_Config.m_LegColor, m_Config.m_LegSize);
        m_pPaceFont = new Font(pugi::as_wide(m_Config.m_PaceFont).c_str(), m_Config.m_PaceSize);
        m_pPaceBrush = new SolidBrush(m_Config.m_PaceColor); 
		m_pPulseFont = new Font(pugi::as_wide(m_Config.m_PulseFont).c_str(), m_Config.m_PulseSize);
		m_pPulseBrush = new SolidBrush(m_Config.m_PulseColor);
		m_pCommFont = new Font(pugi::as_wide(m_Config.m_CommFont).c_str(), m_Config.m_CommSize);
		m_pCommBrush = new SolidBrush(m_Config.m_CommColor);

    }
    m_Initialized = 
           m_pPathDoc != NULL && !m_pPathDoc->empty() 
        //&& m_pTimeFont != NULL && m_pTimeFont->IsAvailable()
        //&& m_pPaceFont != NULL && m_pPaceFont->IsAvailable()
        //&& m_pTimeBrush != NULL && m_pTimeBrush->GetLastStatus() == Ok
        //&& m_pTextBrush != NULL && m_pTextBrush->GetLastStatus() == Ok
        //&& m_pPaceBrush != NULL && m_pPaceBrush->GetLastStatus() == Ok
        && m_pMap != NULL && m_pMap->GetLastStatus() == Ok
        //&& m_pLogo != NULL && m_pLogo->GetLastStatus() == Ok
        //&& m_pPointer != NULL && m_pPointer->GetLastStatus() == Ok
        //&& m_pPenTail != NULL && m_pPenTail->GetLastStatus() == Ok
        //&& m_pPenLeg != NULL && m_pPenLeg->GetLastStatus() == Ok
        ;
    if (!m_Initialized) DeleteVars();
}

void RouteFilter::Start() 
{
    if (!m_Initialized)
    {
        CreateVars();
    }
}

void RouteFilter::End()
{
    DeleteVars();
}

void RouteFilter::Run() 
{
    if (!m_Initialized) 
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
    //create bitmap on frame!
    Bitmap *pbmp = new Bitmap(w, h, pitch, PixelFormat32bppRGB, (BYTE*)data);
    return pbmp;

}

void RouteFilter::ApplyRGB32(Bitmap *pbmp, void *data, uint32 pitch, uint32 w, uint32 h)
{
    //do nothing
}

bool RouteFilter::Configure(VDXHWND hwnd) 
{
    RouteFilterDialog dlg(m_Config, fa->ifp);

	bool ret = dlg.Show((HWND)hwnd);

    if (ret)
    {
        DeleteVars();
        CreateVars();
    }
    return ret;
}

void RouteFilter::DrawRoute(Bitmap *pbmp, uint32 ms) 
{
    //calculate current run time
    int32 time_run = ms - m_Config.m_PathOffset*1000;
    if (time_run < 0)
    {
        time_run = 0;
    }

    //find current sample by run time
    pugi::xml_node current_sample = m_LastSample;
    if (current_sample.empty())
    {
        //get first sample
        current_sample = m_pPathDoc->child("Route").child("Segment").first_child();
    }
    //go forward
    double elapsed_time = current_sample.attribute("elapsedTime").as_double();
    while(time_run > elapsed_time*1000)
    {
        current_sample = current_sample.next_sibling();
        if (current_sample.empty())
        {
            //end of path reached
            current_sample = m_pPathDoc->child("Route").child("Segment").last_child();
            break;
        }
        elapsed_time = current_sample.attribute("elapsedTime").as_double();
    }
    //go to previous sample
    if (!current_sample.previous_sibling().empty())
    {
        current_sample = current_sample.previous_sibling();
        elapsed_time = current_sample.attribute("elapsedTime").as_double();
    }
    //go backward
    while(time_run < elapsed_time*1000)
    {
        current_sample = current_sample.previous_sibling();
        if (current_sample.empty())
        {
            //begin of path reaches
            current_sample = m_pPathDoc->child("Route").child("Segment").first_child();;
            break;
        }
        elapsed_time = current_sample.attribute("elapsedTime").as_double();
    }

    //test if sample change
    //if (m_LastSample.empty() || m_LastSample != current_sample)
    {
        //create bitmap
        // delete m_pLastBmp;
        // m_pLastBmp = new Bitmap(pbmp->GetWidth(), pbmp->GetHeight(), PixelFormat32bppARGB);

        //draw into bitmap
        //Graphics graphics(m_pLastBmp);
        Graphics graphics(pbmp);
        Gdiplus::Status  status = Gdiplus::Status::Ok;
        //Color clear_color(0,0,0,0);
        //graphics.Clear(clear_color);

        //graphics.FillRectangle(m_pTextBrush, m_Config.m_TextX, m_Config.m_TextY, m_Config.m_TextWidth, m_Config.m_TextHeight);

        //draw time text NOTE: samples must be each second!!! in other case time will be displayed with gaps
        WCHAR wstr[256];
        uint32 hour = (time_run/1000)/3600;
        uint32 min = ((time_run/1000)%3600)/60;
        uint32 sec = (time_run/1000)%60;
        wsprintfW(wstr, L"%02i:%02i'%02i", hour, min, sec);
        graphics.DrawString(wstr, wcslen(wstr), m_pTimeFont, PointF(m_Config.m_TextX + m_Config.m_TimeX, m_Config.m_TextY + m_Config.m_TimeY), m_pTimeBrush); 

        wsprintfW(wstr, L"%i", current_sample.attribute("heartRate").as_int());
        graphics.DrawString(wstr, wcslen(wstr), m_pPulseFont, PointF(m_Config.m_TextX + m_Config.m_PulseX, m_Config.m_TextY + m_Config.m_PulseY), m_pPulseBrush); 

        //draw pace
        int pace_m = current_sample.attribute("pace").as_int();
        if (m_Config.m_PaceAvg > 1)  //NOTE: tail by samples, not by seconds
        {
            pugi::xml_node sample = current_sample.previous_sibling();
            for(int i = 1; i < m_Config.m_PaceAvg; i++)
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
            pace_m /= m_Config.m_PaceAvg;
        }
        if (pace_m > 30*60)
        {
            pace_m = 0;
        }
        int pace_s = pace_m%60;
        pace_m /=60;
        wsprintfW(wstr, L"%i'%02i", pace_m, pace_s); 
        graphics.DrawString(wstr, wcslen(wstr), m_pPaceFont, PointF(m_Config.m_TextX + m_Config.m_PaceX, m_Config.m_TextY + m_Config.m_PaceY), m_pPaceBrush);

		//draw comments
		if (m_pCommBrush != NULL && m_pCommFont != NULL)
		{
			for (auto& comment : m_Config.m_Comments)
			{
				if (ms / 1000 > comment.m_Time &&
					ms / 1000 < comment.m_Time + comment.m_Dur)
				{
					RectF rc(m_Config.m_CommX, m_Config.m_CommY, m_Config.m_CommW, m_Config.m_CommH);
					std::wstring draw_me = pugi::as_wide(comment.m_Text.c_str());
					graphics.DrawString
						(draw_me.c_str(),
						draw_me.length(),
						m_pCommFont, 
						rc, 
						StringFormat::GenericDefault(),
						m_pCommBrush);
				}
			}

		}

        //draw logo
        if (m_pLogo)
        {
            ImageAttributes ImgAttr;
            ColorMatrix ClrMatrix = { 
                    1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                    0.0f, 1.0f, 0.0f, 0.0f, 0.0f,
                    0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
                    0.0f, 0.0f, 0.0f, m_Config.m_LogoOpaque/(100.0f), 0.0f,
                    0.0f, 0.0f, 0.0f, 0.0f, 1.0f
            };
            ImgAttr.SetColorMatrix(&ClrMatrix, ColorMatrixFlagsDefault, ColorAdjustTypeBitmap);
            Rect    destination(m_Config.m_LogoX, m_Config.m_LogoY, m_pLogo->GetWidth(), m_pLogo->GetHeight());
            status = graphics.DrawImage(m_pLogo, destination, 0, 0, m_pLogo->GetWidth(), m_pLogo->GetHeight(), UnitPixel, &ImgAttr);
            if (status != Gdiplus::Status::Ok) {
                // TODO: show message
                FILE* pFile = fopen("log.txt", "a");
                fprintf(pFile, "Status 1 %i\n", status);
                fclose(pFile);
                return;
            }
        }

        int32 last_lap = m_LastSample.attribute("lapNumber").as_int();
        int32 cur_lap = current_sample.attribute("lapNumber").as_int();

        bool leg_position_found = false;

        if (last_lap != cur_lap)
        {
            //rebuild leg data

            //1. collect all image positions
            m_LegPoints.clear();
            m_LegTimes.clear();

            pugi::xml_node sample = current_sample;
            //find start of leg, remember leg position
            m_LegPosition = 0;
            leg_position_found = true;
            while(!sample.previous_sibling().empty() && sample.previous_sibling().attribute("lapNumber").as_int() == cur_lap)
            {
                sample = sample.previous_sibling();
                m_LegPosition++;
            }
            //add all leg points to array
            while(!sample.empty() && sample.attribute("lapNumber").as_int() == cur_lap)
            {
                m_LegPoints.push_back(PointF(sample.attribute("imageX").as_int(), sample.attribute("imageY").as_int()));
                m_LegTimes.push_back(sample.attribute("elapsedTime").as_double());
                sample = sample.next_sibling();
            }

            //2. build matrix
            int samples = m_LegPoints.size();
            if (samples > 1)
            {
                PointF start(m_LegPoints[0].X, m_LegPoints[0].Y);
                PointF end(m_LegPoints[samples-1].X, m_LegPoints[samples-1].Y);
                PointF vector(end.X - start.X, end.Y - start.Y);
            
                if (fabs(vector.X) > 1 || fabs(vector.Y) > 1)
                {
					PointF leg_center((end.X + start.X) / 2, (end.Y + start.Y) / 2);

					double vector_size = sqrt(vector.X*vector.X + vector.Y*vector.Y);

					double angle = atan2((double)(vector.Y), (double)(vector.X));
                    double leg_direction = 90.0+180.0*(angle)/M_PI;

                    delete m_pLegMatrix;
                    m_pLegMatrix = new Matrix();
                    m_pLegMatrix->RotateAt(-leg_direction, leg_center);

                    //rotate test points
                    std::vector<PointF> testArr = m_LegPoints;
                    m_pLegMatrix->TransformPoints(testArr.data(), samples);

                    //find bounds
                    REAL left = testArr[0].X - leg_center.X;
                    REAL top = testArr[0].Y - leg_center.Y;
                    REAL right = left;
                    REAL bottom = top;
                    for(int i = 0; i < samples; i++)
                    {
                        left = min(left, testArr[i].X - leg_center.X);
                        top = min(top, testArr[i].Y - leg_center.Y);
                        right = max(right, testArr[i].X - leg_center.X);
                        bottom = max(bottom, testArr[i].Y - leg_center.Y);
                    }
                
                    double scale = (m_Config.m_LegHeight - m_Config.m_LegMargins*2)/vector_size;
                    if (fabs(left) > 0.01)
                    {
                        scale = min(scale, (m_Config.m_LegWidth/2.0 - m_Config.m_LegMargins)/fabs(left));
                    }
                    if (fabs(right) > 0.01)
                    {
                        scale = min(scale, (m_Config.m_LegWidth/2.0 - m_Config.m_LegMargins)/fabs(right));
                    }
                    if (fabs(top) > 0.01)
                    {
                        scale = min(scale, (m_Config.m_LegHeight/2.0 - m_Config.m_LegMargins)/fabs(top));
                    }
                    if (fabs(bottom) > 0.01)
                    {
                        scale = min(scale, (m_Config.m_LegHeight/2.0 - m_Config.m_LegMargins)/fabs(bottom));
                    }
                    
                    if (scale > 1)
                    {
                        scale = 1;
                    }

                    PointF view_center(m_Config.m_LegX + m_Config.m_LegWidth/2, m_Config.m_LegY + m_Config.m_LegHeight/2);

                    m_pLegMatrix->Scale(scale, scale, MatrixOrderAppend);

                    m_pLegMatrix->Translate(view_center.X-leg_center.X*scale, view_center.Y-leg_center.Y*scale, MatrixOrderAppend);

                    m_pLegMatrix->TransformPoints(m_LegPoints.data(), samples);

                }
            }
        }

        //draw leg
        if (m_pMap && m_LegPoints.size() > 1)
        {
            GraphicsState state = graphics.Save();
            
            //TODO: set clip by region
            Rect clip_rect(m_Config.m_LegX, m_Config.m_LegY, m_Config.m_LegWidth, m_Config.m_LegHeight);
            graphics.SetClip(clip_rect);

            REAL t1[6];
            m_pLegMatrix->GetElements(t1);
            graphics.SetTransform(m_pLegMatrix);

            ColorMatrix ClrMatrix = { 
                    1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                    0.0f, 1.0f, 0.0f, 0.0f, 0.0f,
                    0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
                    0.0f, 0.0f, 0.0f, m_Config.m_LegOpaque/(100.0f), 0.0f,
                    0.0f, 0.0f, 0.0f, 0.0f, 1.0f
            };
            ImageAttributes ImgAttr;
            ImgAttr.SetColorMatrix(&ClrMatrix, ColorMatrixFlagsDefault, ColorAdjustTypeBitmap);

            Rect    destination(0, 0, m_pMap->GetWidth(), m_pMap->GetHeight());
            status = graphics.DrawImage(m_pMap, destination, 0, 0, m_pMap->GetWidth(), m_pMap->GetHeight(), UnitPixel, &ImgAttr);
            if (status != Gdiplus::Status::Ok) {
                // TODO: show message
                FILE* pFile = fopen("log.txt", "a");
                fprintf(pFile, "Status 2 %i\n", status);
                fclose(pFile);
                return;
            }

            graphics.Restore(state);

            if (!leg_position_found)
            {
                if (!m_LastSample.empty() && m_LastSample.next_sibling() == current_sample)
                {
                        m_LegPosition++;
                }
                else
                {
                    //find leg position
                    m_LegPosition = 0;
                    for(int i = 0; i < m_LegPoints.size() && m_LegTimes[i] < elapsed_time; i++, m_LegPosition++)
                    {
                        //TODO: binary search;
                    }
                }
            }
            if (m_LegPosition > 1) {
                status = graphics.DrawLines(m_pPenLeg, m_LegPoints.data(), m_LegPosition);
                if (status != Gdiplus::Status::Ok) {
                    // TODO: show message
                    FILE* pFile = fopen("log.txt", "a");
                    fprintf(pFile, "Status 3 %i, size %i, pos %i\n", status, m_LegPoints.size(), m_LegPosition);
                    fclose(pFile);
                    return;
                }
            }
        }

        //draw current position
        if (m_pMap && m_Config.m_PosWidth > 0 && m_Config.m_PosHeight > 0)
        {
            GraphicsState state = graphics.Save();

            int32 image_x = current_sample.attribute("imageX").as_int();
            int32 image_y = current_sample.attribute("imageY").as_int();
            double head_direction = current_sample.attribute("direction").as_double();

            //TODO: clip by region
            Rect clip_rect(m_Config.m_PosX, m_Config.m_PosY, m_Config.m_PosWidth, m_Config.m_PosHeight);
            graphics.SetClip(clip_rect);

            Matrix rotate_at_map;
            PointF center(m_Config.m_PosX + m_Config.m_PosWidth/2, m_Config.m_PosY + m_Config.m_PosHeight/2);
            PointF image_pos(image_x, image_y);
            rotate_at_map.RotateAt(-head_direction, image_pos);
            rotate_at_map.Translate(center.X-image_x, center.Y-image_y, MatrixOrderAppend);
            graphics.SetTransform(&rotate_at_map);

            ColorMatrix ClrMatrix = { 
                    1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                    0.0f, 1.0f, 0.0f, 0.0f, 0.0f,
                    0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
                    0.0f, 0.0f, 0.0f, m_Config.m_PosOpaque/(100.0f), 0.0f,
                    0.0f, 0.0f, 0.0f, 0.0f, 1.0f
            };
            ImageAttributes ImgAttr;
            ImgAttr.SetColorMatrix(&ClrMatrix, ColorMatrixFlagsDefault, ColorAdjustTypeBitmap);

            Rect    destination(0, 0, m_pMap->GetWidth(), m_pMap->GetHeight());
            status = graphics.DrawImage(m_pMap, destination, 0, 0, m_pMap->GetWidth(), m_pMap->GetHeight(), UnitPixel, &ImgAttr);
            if (status != Status::Ok) {
                // TODO: show message
                FILE* pFile = fopen("log.txt", "a");
                fprintf(pFile, "Status 4 %i\n", status);
                fclose(pFile);
                return;
            }

            //draw tail
            {
                int tail = m_Config.m_PosTail;
                int last_x = image_x;
                int last_y = image_y;
                pugi::xml_node sample = current_sample.previous_sibling();
                while(tail-- > 0)
                {
                    if (sample.empty())
                    {
                        break;
                    }
                    int new_img_x = sample.attribute("imageX").as_int();
                    int new_img_y = sample.attribute("imageY").as_int();
                    graphics.DrawLine(m_pPenTail, last_x, last_y, new_img_x, new_img_y);
                    last_x = new_img_x;
                    last_y = new_img_y;
                    sample = sample.previous_sibling();
                }
            }

            graphics.Restore(state);

            //draw pointer
            {
                ImageAttributes ImgAttr;
                ColorMatrix ClrMatrix = { 
                        1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                        0.0f, 1.0f, 0.0f, 0.0f, 0.0f,
                        0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
                        0.0f, 0.0f, 0.0f, m_Config.m_PointerOpaque/(100.0f), 0.0f,
                        0.0f, 0.0f, 0.0f, 0.0f, 1.0f
                };
                ImgAttr.SetColorMatrix(&ClrMatrix, ColorMatrixFlagsDefault, ColorAdjustTypeBitmap);
                Rect    destination(m_Config.m_PosX + m_Config.m_PosWidth/2 - m_pPointer->GetWidth()/2, m_Config.m_PosY + m_Config.m_PosHeight/2 - m_pPointer->GetHeight()/2, m_pPointer->GetWidth(), m_pPointer->GetHeight());
                status = graphics.DrawImage(m_pPointer, destination, 0, 0, m_pPointer->GetWidth(), m_pPointer->GetHeight(), UnitPixel, &ImgAttr);
                if (status != Status::Ok) {
                    // TODO: show message
                    FILE* pFile = fopen("log.txt", "a");
                    fprintf(pFile, "Status 5 %i\n", status);
                    fclose(pFile);
                    return;
                }
            }
        }
        m_LastSample = current_sample;
    }

    //draw last bitmap
    //if (m_pLastBmp)
    //{
    //    Graphics gr_to(pbmp);
    //    gr_to.DrawImage(m_pLastBmp, 0, 0);
    //}

    return;
}

void RouteFilter::ScriptConfig(IVDXScriptInterpreter *isi, const VDXScriptValue *argv, int argc) 
{
    std::string str(*argv[0].asString());
    RouteFilterConfig cfg_new;
    if (cfg_new.From_ConfigString(str))
    {
        m_Config = cfg_new;
    }
    else
    {
        cfg_new.m_File = *argv[0].asString();
        pugi::xml_document doc;
        if (doc.load_file(cfg_new.m_File.c_str()) && cfg_new.FromXml(doc))
        {
            m_Config = cfg_new;
        }
    }
}

void RouteFilter::GetSettingString(char *buf, int maxlen) 
{
	SafePrintf(buf, maxlen, " (\"%s\")", m_Config.m_File.c_str());
}

void RouteFilter::GetScriptString(char *buf, int maxlen) 
{
    std::string str = m_Config.BuildConfigString();

    if (str.size() < maxlen)
    {
        SafePrintf(buf, maxlen, "Config(\"%s\")", str.c_str());
    }
    else
    {
		std::regex slash_regex("\\\\");
		std::string doubled_slashes = std::regex_replace(m_Config.m_File, slash_regex, "\\\\");
	    SafePrintf(buf, maxlen, "Config(\"%s\")", doubled_slashes.c_str());
    }
}

///////////////////////////////////////////////////////////////////////////////

extern VDXFilterDefinition filterDef_RouteAdd = VDXVideoFilterDefinition<RouteFilter>("Vorfol", "Route add", "Add route.");
