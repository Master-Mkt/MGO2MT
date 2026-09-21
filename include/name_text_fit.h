#pragma once
#include <windows.h>
#include <algorithm>
#include <cstdint>
#include <limits>
#include <string_view>

namespace mgo2mt {
// Scoped GDI font selection for a fixed-height name field. Short names use the
// original font object unchanged. Width fitting does not prove glyph coverage.
class NameTextFit {
    HDC dc_;
    HGDIOBJ previous_;
    HFONT fitted_=nullptr;
    SIZE extent_{};
    int limit_;
    bool measured_=false;
public:
    NameTextFit(HDC dc,HFONT original,std::wstring_view text,int width)
        :dc_(dc),previous_(SelectObject(dc,original)),limit_(width) {
        if(width<=0||text.size()>size_t((std::numeric_limits<int>::max)()))return;
        measured_=GetTextExtentPoint32W(dc,text.data(),int(text.size()),&extent_)!=FALSE;
        if(!measured_||extent_.cx<=width)return;
        LOGFONTW lf{};TEXTMETRICW metric{};
        if(!GetObjectW(original,sizeof(lf),&lf)||!GetTextMetricsW(dc,&metric))return;
        lf.lfWidth=(std::max)(1,int(int64_t(metric.tmAveCharWidth)*width/extent_.cx));
        // Keep lfHeight and the original face/quality/weight. Re-measure since
        // font linking and integer cell widths need not scale proportionally.
        for(;;){
            fitted_=CreateFontIndirectW(&lf);if(!fitted_)return;
            SelectObject(dc,fitted_);
            measured_=GetTextExtentPoint32W(dc,text.data(),int(text.size()),&extent_)!=FALSE;
            if(!measured_||extent_.cx<=width||lf.lfWidth==1)return;
            SelectObject(dc,original);DeleteObject(fitted_);fitted_=nullptr;--lf.lfWidth;
        }
    }
    ~NameTextFit(){SelectObject(dc_,previous_);if(fitted_)DeleteObject(fitted_);}
    NameTextFit(const NameTextFit&)=delete;
    NameTextFit& operator=(const NameTextFit&)=delete;
    bool fits()const noexcept{return measured_&&extent_.cx<=limit_;}
    bool adjusted()const noexcept{return fitted_!=nullptr;}
    SIZE extent()const noexcept{return extent_;}
};
}
