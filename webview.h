#ifndef WEBVIEW_H
#define WEBVIEW_H
#include <QObject>
#include <QWidget>
#include <wke.h>
#include <QMap>
#include <QTimer>
#include <QTemporaryDir>
typedef struct _rend_data
{
    QRect rt; //渲染的矩形区域
    void* pixels; //渲染的内存数据
    HDC hDC; //内存设备
    HBITMAP hBitmap; //位图

    _rend_data()
    {
        memset(&rt, 0, sizeof(RECT));
        pixels = NULL;
        hDC = ::CreateCompatibleDC(0);
        hBitmap = NULL;
    }

    ~_rend_data()
    {
        if (hDC)
            DeleteDC(hDC);

        if (hBitmap)
            DeleteObject(hBitmap);
    }

}REND_DATA, *PRENDDATA;


class WebView;
class IWkeCallback
{
public:
    virtual void onWkeTitleChanged(WebView* webView, LPCTSTR title) {}
    virtual void onWkeURLChanged(WebView* webView, LPCTSTR url) {}
    virtual void onWkeAlertBox(WebView* webView, LPCTSTR msg) {}
    virtual bool onWkeNavigation(WebView* webView, wkeNavigationType navigationType, LPCTSTR url) { return false; }
    virtual wkeWebView onWkeCreateView(WebView* webView, wkeNavigationType navType, const wkeString urlStr, const wkeWindowFeatures* features) { return NULL; }
    virtual void onWkeDocumentReady(WebView* webView, void* param) {}
    virtual void onWkeLoadingFinish(WebView* webView, const LPCTSTR url, wkeLoadingResult result, LPCTSTR failedReason) {}
    virtual LPCTSTR onJS2Native(WebView *pWeb, LPCTSTR lpMethod, LPCTSTR lpContent, void *pListenObj) { return NULL; }
};

class WebView:public QWidget
{
    Q_OBJECT
public:
    WebView(QWidget* parent=nullptr);
    ~WebView();
public:
    wkeWebView getWebView();
    //通过hook url访问时，查找本地文件的路径
    void setHookPath(LPCTSTR path);
    std::string getHookPath()const {return m_hookPath;}
    //加载url
    void navigate(LPCTSTR lpUrl);

    //加载html代码
    void loadHtml(LPCTSTR lpHtml);

    //关闭webkit窗口
    void close();

    //页面操作：后退、前进、停止、刷新
    void back();
    void forward();
    void stop();
    void reload();
    void executeJS(QString lpJS);
    bool isDirty();
    //设置页面焦点
    void setPageFocus();
    void setListenObj(void *pListenObj);
    void setWkeCallback(IWkeCallback* pWkeCallback);
public:
    static void initBlink();
    static void uninitBlink();
protected:
    static jsValue JS_CALL jsToNative(jsExecState es);
    // 回调事件
    static void  onWkePaintUpdated(wkeWebView webView, void* param, const HDC hdc, int x, int y, int cx, int cy);
    static void  onWkeTitleChanged(wkeWebView webView, void* param, wkeString title);
    static void  onWkeURLChanged(wkeWebView webView, void* param, wkeString url);
    static void  onWkeAlertBox(wkeWebView webView, void* param, wkeString msg);
    static bool  onWkeNavigation(wkeWebView webView, void* param, wkeNavigationType navigationType, wkeString url);
    static wkeWebView onWkeCreateView(wkeWebView webView, void* param, wkeNavigationType navType, const wkeString urlStr, const wkeWindowFeatures* features);
    static void  onWkeDocumentReady(wkeWebView webView, void* param);
    static void  onWkeLoadingFinish(wkeWebView webView, void* param, const wkeString url, wkeLoadingResult result, const wkeString failedReason);
    static bool onLoadUrlBegin(wkeWebView webView, void* param, const char* url, void *job);
    static void onWkeStartDragging(wkeWebView webView,void* param,wkeWebFrameHandle frame,const wkeWebDragData* data,wkeWebDragOperationsMask mask,const void* image,const wkePoint* dragImageOffset);
private:
    virtual void focusInEvent(QFocusEvent *event) override;
    virtual void focusOutEvent(QFocusEvent *event) override;
    virtual void inputMethodEvent(QInputMethodEvent *event) override;
    virtual void paintEvent(QPaintEvent *event) override;
    virtual void resizeEvent(QResizeEvent *event) override;
    virtual void wheelEvent(QWheelEvent *event) override;
    virtual bool nativeEvent(const QByteArray &eventType, void *message, long *result) override;

private:
    wkeWebView m_pWebView;
    TCHAR m_chCurPageUrl[1024]; //当前页面的url
private:
    QTemporaryDir m_tempDir;
    int m_cursorInfoType;
    std::string m_hookPath;
    bool m_preventBack;
    REND_DATA m_randerData;
    void *m_pListenObj; //监听对象
    IWkeCallback* m_pWkeCallback;	// 回调接口
    static QMap<wkeWebView,WebView*> g_wv;
};

#endif // WEBVIEW_H
