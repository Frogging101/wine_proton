/*
 * Copyright 2008 Jacek Caban for CodeWeavers
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include <stdarg.h>

#define COBJMACROS

#include "windef.h"
#include "winbase.h"
#include "winuser.h"
#include "winreg.h"
#include "ole2.h"
#include "wininet.h"
#include "shlwapi.h"

#include "wine/debug.h"

#include "mshtml_private.h"
#include "binding.h"
#include "resource.h"

WINE_DEFAULT_DEBUG_CHANNEL(mshtml);

static inline HTMLOuterWindow *get_window(HTMLLocation *This)
{
    return CONTAINING_RECORD(This, HTMLOuterWindow, location);
}

static IUri *get_uri(HTMLLocation *This)
{
    return get_window(This)->uri;
}

static HRESULT get_url_components(HTMLLocation *This, URL_COMPONENTSW *url)
{
    const WCHAR *doc_url = get_window(This)->url ? get_window(This)->url : L"about:blank";

    if(!InternetCrackUrlW(doc_url, 0, 0, url)) {
        FIXME("InternetCrackUrlW failed: 0x%08lx\n", GetLastError());
        SetLastError(0);
        return E_FAIL;
    }

    return S_OK;
}

static inline HTMLLocation *impl_from_IHTMLLocation(IHTMLLocation *iface)
{
    return CONTAINING_RECORD(iface, HTMLLocation, IHTMLLocation_iface);
}

static HRESULT WINAPI HTMLLocation_QueryInterface(IHTMLLocation *iface, REFIID riid, void **ppv)
{
    HTMLLocation *This = impl_from_IHTMLLocation(iface);

    TRACE("(%p)->(%s %p)\n", This, debugstr_mshtml_guid(riid), ppv);

    if(IsEqualGUID(&IID_IUnknown, riid)) {
        *ppv = &This->IHTMLLocation_iface;
    }else if(IsEqualGUID(&IID_IHTMLLocation, riid)) {
        *ppv = &This->IHTMLLocation_iface;
    }else if(IsEqualGUID(&IID_IMarshal, riid)) {
        *ppv = NULL;
        FIXME("(%p)->(IID_IMarshal %p)\n", This, ppv);
        return E_NOINTERFACE;
    }else if(dispex_query_interface(&This->dispex, riid, ppv)) {
        return *ppv ? S_OK : E_NOINTERFACE;
    }else {
        *ppv = NULL;
        WARN("(%p)->(%s %p)\n", This, debugstr_mshtml_guid(riid), ppv);
        return E_NOINTERFACE;
    }

    IUnknown_AddRef((IUnknown*)*ppv);
    return S_OK;
}

static ULONG WINAPI HTMLLocation_AddRef(IHTMLLocation *iface)
{
    HTMLLocation *This = impl_from_IHTMLLocation(iface);
    return IHTMLWindow2_AddRef(&get_window(This)->base.IHTMLWindow2_iface);
}

static ULONG WINAPI HTMLLocation_Release(IHTMLLocation *iface)
{
    HTMLLocation *This = impl_from_IHTMLLocation(iface);
    return IHTMLWindow2_Release(&get_window(This)->base.IHTMLWindow2_iface);
}

static HRESULT WINAPI HTMLLocation_GetTypeInfoCount(IHTMLLocation *iface, UINT *pctinfo)
{
    HTMLLocation *This = impl_from_IHTMLLocation(iface);
    return IDispatchEx_GetTypeInfoCount(&This->dispex.IDispatchEx_iface, pctinfo);
}

static HRESULT WINAPI HTMLLocation_GetTypeInfo(IHTMLLocation *iface, UINT iTInfo,
                                              LCID lcid, ITypeInfo **ppTInfo)
{
    HTMLLocation *This = impl_from_IHTMLLocation(iface);
    return IDispatchEx_GetTypeInfo(&This->dispex.IDispatchEx_iface, iTInfo, lcid, ppTInfo);
}

static HRESULT WINAPI HTMLLocation_GetIDsOfNames(IHTMLLocation *iface, REFIID riid,
                                                LPOLESTR *rgszNames, UINT cNames,
                                                LCID lcid, DISPID *rgDispId)
{
    HTMLLocation *This = impl_from_IHTMLLocation(iface);
    return IDispatchEx_GetIDsOfNames(&This->dispex.IDispatchEx_iface, riid, rgszNames, cNames,
            lcid, rgDispId);
}

static HRESULT WINAPI HTMLLocation_Invoke(IHTMLLocation *iface, DISPID dispIdMember,
                            REFIID riid, LCID lcid, WORD wFlags, DISPPARAMS *pDispParams,
                            VARIANT *pVarResult, EXCEPINFO *pExcepInfo, UINT *puArgErr)
{
    HTMLLocation *This = impl_from_IHTMLLocation(iface);
    return IDispatchEx_Invoke(&This->dispex.IDispatchEx_iface, dispIdMember, riid, lcid,
            wFlags, pDispParams, pVarResult, pExcepInfo, puArgErr);
}

static HRESULT WINAPI HTMLLocation_put_href(IHTMLLocation *iface, BSTR v)
{
    HTMLLocation *This = impl_from_IHTMLLocation(iface);

    TRACE("(%p)->(%s)\n", This, debugstr_w(v));

    return navigate_url(get_window(This), v, get_uri(This), BINDING_NAVIGATED);
}

static HRESULT WINAPI HTMLLocation_get_href(IHTMLLocation *iface, BSTR *p)
{
    HTMLLocation *This = impl_from_IHTMLLocation(iface);
    URL_COMPONENTSW url = {sizeof(URL_COMPONENTSW)};
    WCHAR *buf = NULL, *url_path = NULL;
    HRESULT hres, ret;
    DWORD len = 0;
    int i;

    TRACE("(%p)->(%p)\n", This, p);

    if(!p)
        return E_POINTER;

    url.dwSchemeLength = 1;
    url.dwHostNameLength = 1;
    url.dwUrlPathLength = 1;
    url.dwExtraInfoLength = 1;
    hres = get_url_components(This, &url);
    if(FAILED(hres))
        return hres;

    switch(url.nScheme) {
    case INTERNET_SCHEME_FILE:
        {
            /* prepend a slash */
            url_path = malloc((url.dwUrlPathLength + 1) * sizeof(WCHAR));
            if(!url_path)
                return E_OUTOFMEMORY;
            url_path[0] = '/';
            memcpy(url_path + 1, url.lpszUrlPath, url.dwUrlPathLength * sizeof(WCHAR));
            url.lpszUrlPath = url_path;
            url.dwUrlPathLength = url.dwUrlPathLength + 1;
        }
        break;

    case INTERNET_SCHEME_HTTP:
    case INTERNET_SCHEME_HTTPS:
    case INTERNET_SCHEME_FTP:
        if(!url.dwUrlPathLength) {
            /* add a slash if it's blank */
            url_path = url.lpszUrlPath = malloc(sizeof(WCHAR));
            if(!url.lpszUrlPath)
                return E_OUTOFMEMORY;
            url.lpszUrlPath[0] = '/';
            url.dwUrlPathLength = 1;
        }
        break;

    default:
        break;
    }

    /* replace \ with / */
    for(i = 0; i < url.dwUrlPathLength; ++i)
        if(url.lpszUrlPath[i] == '\\')
            url.lpszUrlPath[i] = '/';

    if(InternetCreateUrlW(&url, ICU_ESCAPE, NULL, &len)) {
        FIXME("InternetCreateUrl succeeded with NULL buffer?\n");
        ret = E_FAIL;
        goto cleanup;
    }

    if(GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
        FIXME("InternetCreateUrl failed with error: %08lx\n", GetLastError());
        SetLastError(0);
        ret = E_FAIL;
        goto cleanup;
    }
    SetLastError(0);

    buf = malloc(len * sizeof(WCHAR));
    if(!buf) {
        ret = E_OUTOFMEMORY;
        goto cleanup;
    }

    if(!InternetCreateUrlW(&url, ICU_ESCAPE, buf, &len)) {
        FIXME("InternetCreateUrl failed with error: %08lx\n", GetLastError());
        SetLastError(0);
        ret = E_FAIL;
        goto cleanup;
    }

    *p = SysAllocStringLen(buf, len);
    if(!*p) {
        ret = E_OUTOFMEMORY;
        goto cleanup;
    }

    ret = S_OK;

cleanup:
    free(buf);
    free(url_path);

    return ret;
}

static HRESULT WINAPI HTMLLocation_put_protocol(IHTMLLocation *iface, BSTR v)
{
    HTMLLocation *This = impl_from_IHTMLLocation(iface);
    FIXME("(%p)->(%s)\n", This, debugstr_w(v));
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLLocation_get_protocol(IHTMLLocation *iface, BSTR *p)
{
    HTMLLocation *This = impl_from_IHTMLLocation(iface);
    BSTR protocol, ret;
    unsigned len;
    IUri *uri;
    HRESULT hres;

    TRACE("(%p)->(%p)\n", This, p);

    if(!p)
        return E_POINTER;

    if(!(uri = get_uri(This)))
        return (*p = SysAllocString(L"about:")) ? S_OK : E_OUTOFMEMORY;

    hres = IUri_GetSchemeName(uri, &protocol);
    if(FAILED(hres))
        return hres;
    if(hres == S_FALSE) {
        SysFreeString(protocol);
        *p = NULL;
        return S_OK;
    }

    len = SysStringLen(protocol);
    ret = SysAllocStringLen(protocol, len+1);
    SysFreeString(protocol);
    if(!ret)
        return E_OUTOFMEMORY;

    ret[len] = ':';
    *p = ret;
    return S_OK;
}

static HRESULT WINAPI HTMLLocation_put_host(IHTMLLocation *iface, BSTR v)
{
    HTMLLocation *This = impl_from_IHTMLLocation(iface);
    FIXME("(%p)->(%s)\n", This, debugstr_w(v));
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLLocation_get_host(IHTMLLocation *iface, BSTR *p)
{
    HTMLLocation *This = impl_from_IHTMLLocation(iface);
    URL_COMPONENTSW url = {sizeof(URL_COMPONENTSW)};
    HRESULT hres;

    TRACE("(%p)->(%p)\n", This, p);

    if(!p)
        return E_POINTER;

    url.dwHostNameLength = 1;
    hres = get_url_components(This, &url);
    if(FAILED(hres))
        return hres;

    if(!url.dwHostNameLength){
        *p = NULL;
        return S_OK;
    }

    if(url.nPort) {
        /* <hostname>:<port> */
        DWORD len, port_len;
        WCHAR portW[6];
        WCHAR *buf;

        port_len = swprintf(portW, ARRAY_SIZE(portW), L"%u", url.nPort);
        len = url.dwHostNameLength + 1 /* ':' */ + port_len;
        buf = *p = SysAllocStringLen(NULL, len);
        memcpy(buf, url.lpszHostName, url.dwHostNameLength * sizeof(WCHAR));
        buf[url.dwHostNameLength] = ':';
        memcpy(buf + url.dwHostNameLength + 1, portW, port_len * sizeof(WCHAR));
    }else
        *p = SysAllocStringLen(url.lpszHostName, url.dwHostNameLength);

    if(!*p)
        return E_OUTOFMEMORY;
    return S_OK;
}

static HRESULT WINAPI HTMLLocation_put_hostname(IHTMLLocation *iface, BSTR v)
{
    HTMLLocation *This = impl_from_IHTMLLocation(iface);
    FIXME("(%p)->(%s)\n", This, debugstr_w(v));
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLLocation_get_hostname(IHTMLLocation *iface, BSTR *p)
{
    HTMLLocation *This = impl_from_IHTMLLocation(iface);
    BSTR hostname;
    IUri *uri;
    HRESULT hres;

    TRACE("(%p)->(%p)\n", This, p);

    if(!p)
        return E_POINTER;

    if(!(uri = get_uri(This))) {
        *p = NULL;
        return S_OK;
    }

    hres = IUri_GetHost(uri, &hostname);
    if(hres == S_OK) {
        *p = hostname;
    }else if(hres == S_FALSE) {
        SysFreeString(hostname);
        *p = NULL;
    }else {
        return hres;
    }

    return S_OK;
}

static HRESULT WINAPI HTMLLocation_put_port(IHTMLLocation *iface, BSTR v)
{
    HTMLLocation *This = impl_from_IHTMLLocation(iface);
    FIXME("(%p)->(%s)\n", This, debugstr_w(v));
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLLocation_get_port(IHTMLLocation *iface, BSTR *p)
{
    HTMLLocation *This = impl_from_IHTMLLocation(iface);
    DWORD port;
    IUri *uri;
    HRESULT hres;

    TRACE("(%p)->(%p)\n", This, p);

    if(!p)
        return E_POINTER;

    if(!(uri = get_uri(This))) {
        *p = NULL;
        return S_OK;
    }

    hres = IUri_GetPort(uri, &port);
    if(FAILED(hres))
        return hres;

    if(hres == S_OK) {
        WCHAR buf[12];

        swprintf(buf, ARRAY_SIZE(buf), L"%u", port);
        *p = SysAllocString(buf);
    }else {
        *p = SysAllocStringLen(NULL, 0);
    }

    if(!*p)
        return E_OUTOFMEMORY;
    return S_OK;
}

static HRESULT WINAPI HTMLLocation_put_pathname(IHTMLLocation *iface, BSTR v)
{
    HTMLLocation *This = impl_from_IHTMLLocation(iface);
    FIXME("(%p)->(%s)\n", This, debugstr_w(v));
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLLocation_get_pathname(IHTMLLocation *iface, BSTR *p)
{
    HTMLLocation *This = impl_from_IHTMLLocation(iface);
    BSTR path;
    IUri *uri;
    HRESULT hres;

    TRACE("(%p)->(%p)\n", This, p);

    if(!p)
        return E_POINTER;

    if(!(uri = get_uri(This)))
        return (*p = SysAllocString(L"blank")) ? S_OK : E_OUTOFMEMORY;

    hres = IUri_GetPath(uri, &path);
    if(FAILED(hres))
        return hres;

    if(hres == S_FALSE) {
        SysFreeString(path);
        path = NULL;
    }

    *p = path;
    return S_OK;
}

static HRESULT WINAPI HTMLLocation_put_search(IHTMLLocation *iface, BSTR v)
{
    HTMLLocation *This = impl_from_IHTMLLocation(iface);
    FIXME("(%p)->(%s)\n", This, debugstr_w(v));
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLLocation_get_search(IHTMLLocation *iface, BSTR *p)
{
    HTMLLocation *This = impl_from_IHTMLLocation(iface);
    BSTR query;
    IUri *uri;
    HRESULT hres;

    TRACE("(%p)->(%p)\n", This, p);

    if(!p)
        return E_POINTER;

    if(!(uri = get_uri(This))) {
        *p = NULL;
        return S_OK;
    }

    hres = IUri_GetQuery(uri, &query);
    if(hres == S_OK) {
        *p = query;
    }else if(hres == S_FALSE) {
        SysFreeString(query);
        *p = NULL;
    }else {
        return hres;
    }

    return S_OK;
}

static HRESULT WINAPI HTMLLocation_put_hash(IHTMLLocation *iface, BSTR v)
{
    HTMLLocation *This = impl_from_IHTMLLocation(iface);
    WCHAR *hash = v;
    HRESULT hres;

    TRACE("(%p)->(%s)\n", This, debugstr_w(v));

    if(hash[0] != '#') {
        unsigned size = (1 /* # */ + wcslen(v) + 1) * sizeof(WCHAR);
        if(!(hash = malloc(size)))
            return E_OUTOFMEMORY;
        hash[0] = '#';
        memcpy(hash + 1, v, size - sizeof(WCHAR));
    }

    hres = navigate_url(get_window(This), hash, get_uri(This), BINDING_NAVIGATED);

    if(hash != v)
        free(hash);
    return hres;
}

static HRESULT WINAPI HTMLLocation_get_hash(IHTMLLocation *iface, BSTR *p)
{
    HTMLLocation *This = impl_from_IHTMLLocation(iface);
    BSTR hash;
    IUri *uri;
    HRESULT hres;

    TRACE("(%p)->(%p)\n", This, p);

    if(!p)
        return E_POINTER;

    if(!(uri = get_uri(This))) {
        *p = NULL;
        return S_OK;
    }

    hres = IUri_GetFragment(uri, &hash);
    if(hres == S_OK) {
        *p = hash;
    }else if(hres == S_FALSE) {
        SysFreeString(hash);
        *p = NULL;
    }else {
        return hres;
    }

    return S_OK;
}

static HRESULT WINAPI HTMLLocation_reload(IHTMLLocation *iface, VARIANT_BOOL flag)
{
    HTMLLocation *This = impl_from_IHTMLLocation(iface);

    TRACE("(%p)->(%x)\n", This, flag);

    /* reload is supposed to fail if called from a script with different origin, but IE doesn't care */
    if(!is_main_content_window(get_window(This))) {
        FIXME("Unsupported on iframe\n");
        return E_NOTIMPL;
    }

    return reload_page(get_window(This));
}

static HRESULT WINAPI HTMLLocation_replace(IHTMLLocation *iface, BSTR bstr)
{
    HTMLLocation *This = impl_from_IHTMLLocation(iface);

    TRACE("(%p)->(%s)\n", This, debugstr_w(bstr));

    return navigate_url(get_window(This), bstr, get_uri(This), BINDING_NAVIGATED | BINDING_REPLACE);
}

static HRESULT WINAPI HTMLLocation_assign(IHTMLLocation *iface, BSTR bstr)
{
    HTMLLocation *This = impl_from_IHTMLLocation(iface);
    TRACE("(%p)->(%s)\n", This, debugstr_w(bstr));
    return IHTMLLocation_put_href(iface, bstr);
}

static HRESULT WINAPI HTMLLocation_toString(IHTMLLocation *iface, BSTR *String)
{
    HTMLLocation *This = impl_from_IHTMLLocation(iface);

    TRACE("(%p)->(%p)\n", This, String);

    return IHTMLLocation_get_href(&This->IHTMLLocation_iface, String);
}

static const IHTMLLocationVtbl HTMLLocationVtbl = {
    HTMLLocation_QueryInterface,
    HTMLLocation_AddRef,
    HTMLLocation_Release,
    HTMLLocation_GetTypeInfoCount,
    HTMLLocation_GetTypeInfo,
    HTMLLocation_GetIDsOfNames,
    HTMLLocation_Invoke,
    HTMLLocation_put_href,
    HTMLLocation_get_href,
    HTMLLocation_put_protocol,
    HTMLLocation_get_protocol,
    HTMLLocation_put_host,
    HTMLLocation_get_host,
    HTMLLocation_put_hostname,
    HTMLLocation_get_hostname,
    HTMLLocation_put_port,
    HTMLLocation_get_port,
    HTMLLocation_put_pathname,
    HTMLLocation_get_pathname,
    HTMLLocation_put_search,
    HTMLLocation_get_search,
    HTMLLocation_put_hash,
    HTMLLocation_get_hash,
    HTMLLocation_reload,
    HTMLLocation_replace,
    HTMLLocation_assign,
    HTMLLocation_toString
};

static const tid_t HTMLLocation_iface_tids[] = {
    IHTMLLocation_tid,
    0
};
dispex_static_data_t HTMLLocation_dispex = {
    L"Location",
    NULL,
    PROTO_ID_HTMLLocation,
    DispHTMLLocation_tid,
    HTMLLocation_iface_tids
};

void HTMLLocation_Init(HTMLOuterWindow *window)
{
    compat_mode_t compat_mode = dispex_compat_mode(&window->base.inner_window->event_target.dispex);

    window->location.IHTMLLocation_iface.lpVtbl = &HTMLLocationVtbl;

    init_dispatch(&window->location.dispex, (IUnknown*)&window->location.IHTMLLocation_iface, &HTMLLocation_dispex,
                  window->base.inner_window, min(compat_mode, COMPAT_MODE_IE8));
}
