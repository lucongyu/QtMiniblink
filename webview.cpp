#include "webview.h"
#include <QApplication>
#include <QPainter>
#include <QBitmap>
#include <QPaintEvent>
#include <windowbase.h>
#include <QMouseEvent>
#include <QInputMethod>
#include <QCoreApplication>
#include <string>
#if WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlwapi.h>
#include <wingdi.h>
#include <QtWinExtras/qwinfunctions.h>
#include <cstringt.h>
#endif

QMap<wkeWebView,WebView*> WebView::g_wv= QMap<wkeWebView,WebView*>();


WebView::WebView(QWidget* parent):m_preventBack(false)
{
    setFocusPolicy(Qt::FocusPolicy::StrongFocus);
    setAttribute(Qt::WA_InputMethodEnabled,true);
    memset(m_chCurPageUrl, 0, sizeof(m_chCurPageUrl));
    m_pWebView = wkeCreateWebView();
    g_wv[m_pWebView] = this;
    m_pListenObj = NULL;
    m_pWkeCallback = NULL;
    m_tempDir.setAutoRemove(true);
    if(m_tempDir.isValid()){
        QString ps =m_tempDir.path();
        qDebug("%s",ps.toStdString().c_str());
        wchar_t wps[MAX_PATH];
        ps.toWCharArray(wps);
        wkeSetCookieJarFullPath(m_pWebView,wps);
        wkeSetLocalStorageFullPath(m_pWebView, wps);
    }
    wkeSetCookieEnabled(m_pWebView, true);
    wkeOnPaintUpdated(m_pWebView, onWkePaintUpdated, this);
    wkeOnTitleChanged(m_pWebView, onWkeTitleChanged, this);
    wkeOnURLChanged(m_pWebView, onWkeURLChanged, this);
    wkeOnNavigation(m_pWebView, onWkeNavigation, this);
    wkeOnCreateView(m_pWebView, onWkeCreateView, this);
    wkeOnLoadUrlBegin(m_pWebView,onLoadUrlBegin, this);
    wkeOnDocumentReady(m_pWebView, onWkeDocumentReady, this);
    wkeOnLoadingFinish(m_pWebView, onWkeLoadingFinish, this);
    wkeSetTransparent(m_pWebView, false);
    wkeSetDragEnable(m_pWebView,true);
    wkeOnStartDragging(m_pWebView,onWkeStartDragging,this);
    // 设置UA
    wkeSetUserAgent(m_pWebView, "Mozilla/5.0 (Windows NT 6.1) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/69.0.2228.0 Safari/537.36");
    // 设置名称
    wkeSetHandle(m_pWebView,(HWND)this->winId());
    wkeSetName(m_pWebView, "WebView");
}

WebView::~WebView(){
    QMap<wkeWebView, WebView*>::Iterator iter = g_wv.find(m_pWebView);
    if (iter != g_wv.end())
    {
        g_wv.erase(iter);
    }
    wkeDestroyWebView(m_pWebView);
}


void WebView::initBlink(){
    static bool isInitialized = ::wkeIsInitialize == NULL ? false : (::wkeIsInitialize());
    if (!isInitialized) {
        QString strResourcePath =QApplication::applicationDirPath();
        QString mbPath = strResourcePath + QString("/miniblink.dll");
        const std::string path=mbPath.toLocal8Bit().toStdString();
        wkeSetWkeDllPath(path.c_str());
        // 初始化
        wkeInitialize();
        //绑定全局函数
        jsBindFunction("jsToNative", jsToNative, 2);
    }
}


void WebView::uninitBlink(){
    wkeFinalize();
}


wkeWebView WebView::getWebView()
{
    return m_pWebView;
}

void WebView::setHookPath(LPCTSTR path){
    m_hookPath=path;
}

void WebView::navigate(LPCTSTR lpUrl)
{
    wkeLoadURL(m_pWebView, lpUrl);
}

void WebView::loadHtml(LPCTSTR lpHtml)
{
    wkeLoadHTML(m_pWebView, lpHtml);
}

void WebView::close()
{
    wkeDestroyWebView(m_pWebView);
}

void WebView::back()
{
    if (wkeCanGoBack(m_pWebView))
        wkeGoBack(m_pWebView);
}

void WebView::forward()
{
    if (wkeCanGoForward(m_pWebView))
        wkeGoForward(m_pWebView);
}

void WebView::stop()
{
    wkeStopLoading(m_pWebView);
}

void WebView::reload()
{
    wkeReload(m_pWebView);
}


void WebView::executeJS(QString js){
    wkeRunJS(m_pWebView, js.toUtf8());
}
bool WebView::isDirty()
{
    return wkeIsDirty(m_pWebView);
}

void WebView::setPageFocus()
{
    wkeSetFocus(m_pWebView);
}

void WebView::setListenObj(void *pListenObj)
{
    m_pListenObj = pListenObj;
}
void WebView::setWkeCallback(IWkeCallback* pWkeCallback)
{
    m_pWkeCallback = pWkeCallback;
}

void WebView::onWkePaintUpdated(wkeWebView webView, void* param, const HDC hdc, int x, int y, int cx, int cy)
{
    ((WebView*)param)->update();
}

void WebView::onWkeTitleChanged(wkeWebView webView, void* param, wkeString title)
{
    WebView *pWkeUI = (WebView*)param;
    if (!pWkeUI)	return;

    if (pWkeUI->m_pWkeCallback) {
        return pWkeUI->m_pWkeCallback->onWkeTitleChanged(pWkeUI, wkeGetString(title));
    }
}

void WebView::onWkeURLChanged(wkeWebView webView, void* param, wkeString url)
{
    WebView *pWkeUI = (WebView*)param;
    if (!pWkeUI)	return;
    if (pWkeUI->m_pWkeCallback) {
        return pWkeUI->m_pWkeCallback->onWkeURLChanged(pWkeUI, wkeGetString(url));
    }
}

void WebView::onWkeAlertBox(wkeWebView webView, void* param, wkeString msg)
{
    WebView *pWkeUI = (WebView*)param;
    if (!pWkeUI)	return;

    if (pWkeUI->m_pWkeCallback) {
        return pWkeUI->m_pWkeCallback->onWkeAlertBox(pWkeUI, wkeGetString(msg));
    }
}

bool WebView::onWkeNavigation(wkeWebView webView, void* param, wkeNavigationType navigationType, wkeString url)
{
    WebView *pWkeUI = (WebView*)param;
    if (!pWkeUI)	return true;
    LPCTSTR pStrUrl = wkeGetString(url);
    if (!strstr(pStrUrl, TEXT("error.html"))) {
        strcpy(pWkeUI->m_chCurPageUrl, pStrUrl);
    }

    if (pWkeUI->m_pWkeCallback) {
        return pWkeUI->m_pWkeCallback->onWkeNavigation(pWkeUI, navigationType, pStrUrl);
    }
    return true;
}

wkeWebView WebView::onWkeCreateView(wkeWebView webView, void* param, wkeNavigationType navType, const wkeString urlStr, const wkeWindowFeatures* features)
{
    WebView *pWkeUI = (WebView*)param;
    if (!pWkeUI)	return NULL;

    if (pWkeUI->m_pWkeCallback) {
        return pWkeUI->m_pWkeCallback->onWkeCreateView(pWkeUI, navType, urlStr, features);
    }

    return NULL;
}

void WebView::onWkeDocumentReady(wkeWebView webView, void* param)
{
    WebView *pWkeUI = (WebView*)param;
    if (!pWkeUI)	return;

    if (pWkeUI->m_pWkeCallback) {
        return pWkeUI->m_pWkeCallback->onWkeDocumentReady(pWkeUI, param);
    }
}

void WebView::onWkeLoadingFinish(wkeWebView webView, void* param, const wkeString url, wkeLoadingResult result, const wkeString failedReason)
{
    WebView *pWkeUI = (WebView*)param;
    if (!pWkeUI)	return;
    //页面加载失败
    if (result == WKE_LOADING_FAILED) {
        pWkeUI->reload();
    }

    if (pWkeUI->m_pWkeCallback) {
        pWkeUI->m_pWkeCallback->onWkeLoadingFinish(pWkeUI, wkeGetString(url), result, wkeGetString(failedReason));
    }
}

std::string getResourcesPath(const std::string& name)
{
    std::string result;
    std::string temp;
    std::vector<char> path;
    path.resize(MAX_PATH + 1);
    ::GetModuleFileName(nullptr, &path[0], MAX_PATH);
    ::PathRemoveFileSpec(&path[0]);
    temp += &path[0];
    temp += "\\";

    result = temp + name;
    if (!::PathFileExists(result.c_str())) {
        result = temp + "..\\";
        result += name;
    }
    return result;
}
void readJsFile(const char* path, std::vector<char>* buffer)
{
    HANDLE hFile = CreateFile(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (INVALID_HANDLE_VALUE == hFile) {
        qDebug("Invalid file :%s",path);
        return;
    }

    DWORD fileSizeHigh;
    const DWORD bufferSize = ::GetFileSize(hFile, &fileSizeHigh);

    DWORD numberOfBytesRead = 0;
    buffer->resize(bufferSize);
    BOOL b = ::ReadFile(hFile, &buffer->at(0), bufferSize, &numberOfBytesRead, nullptr);
    ::CloseHandle(hFile);
}

bool WebView::onLoadUrlBegin(wkeWebView webView, void* param, const char* url, void *job){
    const char hook_url[] = "http://hook/";
    const char* pos = strstr(url, hook_url);
    if (pos) {
        const utf8* decodeURL = wkeUtilDecodeURLEscape(url);
        if (!decodeURL)
            return false;
        WebView* wv=(WebView*)param;
        std::string urlString(decodeURL);
        std::string localPath = urlString.substr(sizeof(hook_url) - 1);
        std::string path =wv->getHookPath();
        if(path.length()<=0){
            path= getResourcesPath(localPath);
        }
        std::vector<char> buffer;
        readJsFile(path.c_str(), &buffer);
        wkeNetSetData(job, buffer.data(), buffer.size());
        return true;
    }
    return false;
}

void WebView::onWkeStartDragging(wkeWebView webView,void* param,wkeWebFrameHandle frame,const wkeWebDragData* data,wkeWebDragOperationsMask mask,const void* image,const wkePoint* dragImageOffset){
    qDebug("start Dragging");
    POINT start{0,0};
    POINT end{0,0};
    wkeDragTargetEnd(webView,&start,&end,wkeWebDragOperation::wkeWebDragOperationNone);
}

jsValue WebView::jsToNative(jsExecState es)
{
    //查找UI对象
    WebView *pWkeUI = NULL;
    wkeWebView pWke = jsGetWebView(es);
    if (pWke) {
        QMap<wkeWebView, WebView*>::const_iterator iter = g_wv.find(pWke);
        if (iter != g_wv.end()) {
            pWkeUI = iter.value();
        }
    }
    if (pWkeUI) {
        int nArg = jsArgCount(es);
        if (nArg == 2) {
            jsValue arg1 = jsArg(es, 0);
            jsValue arg2 = jsArg(es, 1);
            if (jsIsString(arg1) && jsIsString(arg2)) {
                //需要保证两个参数都为字符串
                char buf1[16 * 1024] = { 0 }, buf2[16 * 1024] = { 0 };
                strncpy_s(buf1, jsToTempString(es, arg1), 16 * 1024 - 1);
                strncpy_s(buf2, jsToTempString(es, arg2), 16 * 1024 - 1);
                LPCTSTR lpArg1 = buf1;
                LPCTSTR lpArg2 = buf2;
                if (strcmp(lpArg1, "refresh") == 0) {
                    //本地刷新
                    pWkeUI->navigate(pWkeUI->m_chCurPageUrl);
                    return jsUndefined();
                }
                if (pWkeUI->m_pWkeCallback) {
                    LPCTSTR lpRet = pWkeUI->m_pWkeCallback->onJS2Native(pWkeUI, lpArg1, lpArg2, pWkeUI->m_pListenObj);
                    if (lpRet) {
                        return jsString(es, lpRet);
                    }
                }
            }
        }
    }
    return jsUndefined();
}
void WebView::focusInEvent(QFocusEvent *event) {
    wkeSetFocus(m_pWebView);
}
void WebView::focusOutEvent(QFocusEvent *event) {
    wkeKillFocus(m_pWebView);
}

void WebView::inputMethodEvent(QInputMethodEvent *event) {
    const QString  stdStr= event->commitString();
    for  (const auto &c:stdStr){
        wkeFireKeyPressEvent(m_pWebView,c.unicode() ,WKE_EXTENDED,false);
    }
}
void WebView::paintEvent(QPaintEvent *event){
    QPainter pt(this);
    if(!pt.isActive())return;
    BITMAPINFO bi;
    if(m_randerData.pixels==NULL){
        memset(&bi, 0, sizeof(bi));
        bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bi.bmiHeader.biWidth = this->width();
        bi.bmiHeader.biHeight = -this->height();
        bi.bmiHeader.biPlanes = 1;
        bi.bmiHeader.biBitCount = 32;
        bi.bmiHeader.biCompression = BI_RGB;
        HBITMAP hbmp = ::CreateDIBSection(0, &bi, DIB_RGB_COLORS, &m_randerData.pixels, NULL, 0);
        ::SelectObject(m_randerData.hDC,hbmp);
        if (m_randerData.hBitmap) {
            DeleteObject(m_randerData.hBitmap);
        }
        m_randerData.hBitmap = hbmp;
    }
    wkePaint(m_pWebView, m_randerData.pixels, 0);

    pt.drawPixmap(event->rect(),QtWin::fromHBITMAP(m_randerData.hBitmap));
}

void WebView::resizeEvent(QResizeEvent *event){
    m_randerData.rt=rect();
    m_randerData.pixels=NULL;
    int width =this->width();
    int height =this->height();
    ::wkeResize(m_pWebView, width, height);
}

void WebView::wheelEvent(QWheelEvent *event) {
    QPoint pt=event->position().toPoint();
    unsigned int flags = 0;
    if (event->modifiers() & Qt::ControlModifier)
        flags |= WKE_CONTROL;
    if (event->modifiers() & Qt::ShiftModifier)
        flags |= WKE_SHIFT;
    if (event->buttons() & Qt::LeftButton)
        flags |= WKE_LBUTTON;
    if (event->buttons() & Qt::MiddleButton)
        flags |= WKE_MBUTTON;
    if (event->buttons() & Qt::RightButton)
        flags |= WKE_RBUTTON;
    wkeFireMouseWheelEvent(m_pWebView, pt.x(), pt.y(), event->angleDelta().y() ,flags);
}

bool WebView::nativeEvent(const QByteArray &eventType, void *message, long *result){
    MSG* pMsg = reinterpret_cast<MSG*>(message);
    WPARAM wParam = pMsg->wParam;
    LPARAM lParam = pMsg->lParam;
    UINT msg = pMsg->message;
    switch (msg) {
    case WM_KEYDOWN:
    {
        unsigned int virtualKeyCode = wParam;
        unsigned int flags = 0;
        if (HIWORD(lParam) & KF_REPEAT)
            flags |= WKE_REPEAT;
        if (HIWORD(lParam) & KF_EXTENDED)
            flags |= WKE_EXTENDED;
        if(virtualKeyCode==VK_BACK)
            m_preventBack=true;
        wkeFireKeyDownEvent(m_pWebView, virtualKeyCode, flags, false);
        break;
    }
    case WM_KEYUP:
    {
        unsigned int virtualKeyCode = wParam;
        unsigned int flags = 0;
        if (HIWORD(lParam) & KF_REPEAT)
            flags |= WKE_REPEAT;
        if (HIWORD(lParam) & KF_EXTENDED)
            flags |= WKE_EXTENDED;

        wkeFireKeyUpEvent(m_pWebView, virtualKeyCode, flags, false);
        break;
    }
    case WM_CHAR:
    {
        TCHAR charCode = wParam;
        unsigned int flags = 0;
        if (HIWORD(lParam) & KF_REPEAT)
            flags |= WKE_REPEAT;
        if (HIWORD(lParam) & KF_EXTENDED)
            flags |= WKE_EXTENDED;
        wkeFireKeyPressEvent(m_pWebView, charCode, flags,false);
        break;
    }
    case WM_LBUTTONDOWN:
    case WM_MBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_LBUTTONDBLCLK:
    case WM_MBUTTONDBLCLK:
    case WM_RBUTTONDBLCLK:
    case WM_LBUTTONUP:
    case WM_MBUTTONUP:
    case WM_RBUTTONUP:
    case WM_MOUSEMOVE:
    {
        m_cursorInfoType = wkeGetCursorInfoType(m_pWebView);
        if (msg == WM_LBUTTONDOWN || msg == WM_MBUTTONDOWN || msg == WM_RBUTTONDOWN) {
            grabMouse();
        } else if (msg == WM_LBUTTONUP || msg == WM_MBUTTONUP || msg == WM_RBUTTONUP) {
            releaseMouse();
        }
        int x = LOWORD(lParam);
        int y = HIWORD(lParam);
        unsigned int flags = 0;
        if (wParam & MK_CONTROL)
            flags |= WKE_CONTROL;
        if (wParam & MK_SHIFT)
            flags |= WKE_SHIFT;
        if (wParam & MK_LBUTTON)
            flags |= WKE_LBUTTON;
        if (wParam & MK_MBUTTON)
            flags |= WKE_MBUTTON;
        if (wParam & MK_RBUTTON)
            flags |= WKE_RBUTTON;
        wkeFireMouseEvent(m_pWebView, msg, x, y, flags);
        break;
    }
    default:
        break;
    }
    return false;
}
