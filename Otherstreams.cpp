/////////////////////////////////////////////////////////////////////////////
// Name:        Otherstreams.cpp
// Purpose:     xz and lmza1 archive streams
// Part of:     4Pane
// Author:      David Hart
// Copyright:   (c) 2019 David Hart
// Licence:     GPL v3
/////////////////////////////////////////////////////////////////////////////

#ifndef NO_LZMA_ARCHIVE_STREAMS

#include "wx/wxprec.h"

#include "wx/wx.h"
#include "wx/utils.h"
#include "wx/intl.h"
#include "wx/log.h"

#if wxUSE_STREAMS

#include "Externs.h"
#include "MyFrame.h"
#include "Otherstreams.h"

XzInputStream::XzInputStream(wxInputStream& Stream, bool use_lzma1 /*= false*/) :  wxFilterInputStream(Stream), m_nBufferPos(0)
{
wxCHECK_RET(LIBLZMA_LOADED, wxT("We shouldn't be here if liblzma isn't loaded!"));

lzma_stream_decoder = (dl_lzma_stream_decoder)(wxGetApp().GetLiblzma()->GetSymbol(wxT("lzma_stream_decoder")));
if (!lzma_stream_decoder) { wxLogWarning(wxT("Failed to load symbol 'lzma_stream_decoder'")); return; }
lzma_alone_decoder = (dl_lzma_alone_decoder)(wxGetApp().GetLiblzma()->GetSymbol(wxT("lzma_alone_decoder")));
if (!lzma_alone_decoder) { wxLogWarning(wxT("Failed to load symbol 'lzma_alone_decoder'")); return; }
lzma_code = (dl_lzma_code)(wxGetApp().GetLiblzma()->GetSymbol(wxT("lzma_code")));
if (!lzma_code) { wxLogWarning(wxT("Failed to load symbol 'lzma_code'")); return; }
lzma_end = (dl_lzma_end)(wxGetApp().GetLiblzma()->GetSymbol(wxT("lzma_end")));
if (!lzma_end) { wxLogWarning(wxT("Failed to load symbol 'lzma_end'")); return; }

m_lzStream = (void*) new lzma_stream;
lzma_stream* lzstr = (lzma_stream*)m_lzStream;

memset(lzstr, 0, sizeof(lzma_stream)); // This is the recommended way of initialising a dynamically allocated lzma_stream

/* initialize xz (or lzma1) decoder */
lzma_ret ret_xz;
if (!use_lzma1)
  ret_xz = lzma_stream_decoder(lzstr, UINT64_MAX, LZMA_TELL_UNSUPPORTED_CHECK /*| LZMA_CONCATENATED*/); // The LZMA_CONCATENATED doesn't seem to be needed
 else
  ret_xz = lzma_alone_decoder(lzstr, UINT64_MAX);

if (ret_xz != LZMA_OK) 
  { delete lzstr;
    wxLogSysError(wxT("Could not initialize %s decompression engine!"), use_lzma1 ? wxString(wxT("lzma1")).c_str():wxString(wxT("xz")).c_str());
  }
}

XzInputStream::~XzInputStream()
{
lzma_stream* lzstr = (lzma_stream*)m_lzStream;
lzma_end(lzstr);

delete lzstr;
}

wxInputStream& XzInputStream::ReadRaw(void* pBuffer, size_t size)
{
  return m_parent_i_stream->Read(pBuffer, size);
}

off_t XzInputStream::TellRawI()
{
  return m_parent_i_stream->TellI();
}

off_t XzInputStream::SeekRawI(off_t pos, wxSeekMode sm)
{
  return 0;
}

size_t XzInputStream::OnSysRead(void* buffer, size_t bufsize)
{
// This is a clone of wxZlibInputStream::OnSysRead, adjusted for xz

lzma_stream* lzstr = (lzma_stream*)m_lzStream;

if (!lzstr)
  m_lasterror = wxSTREAM_READ_ERROR;
if (!IsOk() || !bufsize)
  return 0;

lzma_ret ret_xz = LZMA_OK;
lzstr->next_out = (uint8_t*)buffer;
lzstr->avail_out = bufsize;

while (ret_xz == LZMA_OK && lzstr->avail_out > 0) 
  { if (lzstr->avail_in == 0 && m_parent_i_stream->IsOk()) 
      { m_parent_i_stream->Read(m_pBuffer, IN_BUF_MAX);
        lzstr->next_in = m_pBuffer;
        lzstr->avail_in = m_parent_i_stream->LastRead();
      }
    ret_xz = lzma_code(lzstr, LZMA_RUN);
  }

switch (ret_xz) 
  { case LZMA_OK:   break;

    case LZMA_FINISH:
      if (lzstr->avail_out) {
        // 'Inflate' comment was: "Unread any data taken from past the end of the deflate stream, so that
        // any additional data can be read from the underlying stream (the crc in a gzip for example)"
        // I don't know if this is still relevant in xz, but it doesn't seem to do any harm; perhaps because it's not called
        if (lzstr->avail_in) {
          m_parent_i_stream->Reset();
          m_parent_i_stream->Ungetch(lzstr->next_in, lzstr->avail_in);
          lzstr->avail_in = 0;
        }
        m_lasterror = wxSTREAM_EOF;
      }
      break;

    default:
      wxLogError(_("Decompressing a lzma stream failed"));
      m_lasterror = wxSTREAM_READ_ERROR;
  }

bufsize -= lzstr->avail_out;
m_nBufferPos += bufsize;
return bufsize;
}


XzOutputStream::XzOutputStream(wxOutputStream& Stream, bool use_lzma1 /*= false*/) :  wxFilterOutputStream(Stream)
{
wxCHECK_RET(LIBLZMA_LOADED, wxT("We shouldn't be here if liblzma isn't loaded!"));

lzma_easy_encoder = (dl_lzma_easy_encoder)(wxGetApp().GetLiblzma()->GetSymbol(wxT("lzma_easy_encoder")));
if (!lzma_easy_encoder) { wxLogWarning(wxT("Failed to load symbol 'lzma_easy_encoder'")); return; }
lzma_alone_encoder = (dl_lzma_alone_encoder)(wxGetApp().GetLiblzma()->GetSymbol(wxT("lzma_alone_encoder")));
if (!lzma_alone_encoder) { wxLogWarning(wxT("Failed to load symbol 'lzma_alone_encoder'")); return; }
lzma_lzma_preset = (dl_lzma_lzma_preset)(wxGetApp().GetLiblzma()->GetSymbol(wxT("lzma_lzma_preset")));
if (!lzma_lzma_preset) { wxLogWarning(wxT("Failed to load symbol 'lzma_lzma_preset'")); return; }
lzma_code = (dl_lzma_code)(wxGetApp().GetLiblzma()->GetSymbol(wxT("lzma_code")));
if (!lzma_code) { wxLogWarning(wxT("Failed to load symbol 'lzma_code'")); return; }
lzma_end = (dl_lzma_end)(wxGetApp().GetLiblzma()->GetSymbol(wxT("lzma_end")));
if (!lzma_end) { wxLogWarning(wxT("Failed to load symbol 'lzma_end'")); return; }

m_lzStream = (void*) new lzma_stream;
lzma_stream* lzstr = (lzma_stream*)m_lzStream;

memset(lzstr, 0, sizeof(lzma_stream)); // This is the recommended way of initialising a dynamically allocated lzma_stream

lzma_ret ret_xz;
if (!use_lzma1)
  ret_xz = lzma_easy_encoder(lzstr, 6, LZMA_CHECK_CRC64);
 else
  { lzma_options_lzma options;
    lzma_lzma_preset(&options, 7);
    ret_xz = lzma_alone_encoder(lzstr, &options);
  }

if (ret_xz != LZMA_OK) 
  { delete lzstr;
    wxLogSysError(wxT("Could not initialize %s compression engine!"), use_lzma1 ? wxString(wxT("lzma1")).c_str():wxString(wxT("xz")).c_str());
    
  }
}

XzOutputStream::~XzOutputStream()
{
lzma_stream* lzstr = (lzma_stream*)m_lzStream;
lzma_end(lzstr);

delete lzstr;
}

wxOutputStream& XzOutputStream::WriteRaw(void* pBuffer, size_t size)
{
  return m_parent_o_stream->Write(pBuffer, size);
}

off_t XzOutputStream::TellRawO()
{
  return m_parent_o_stream->TellO();
}

off_t XzOutputStream::SeekRawO(off_t pos, wxSeekMode sm)
{
  return 0;
}

size_t XzOutputStream::OnSysWrite(const void* buffer, size_t bufsize)
{
// This is a clone of the xz compression example, xz_pipe_comp.c, adjusted for a wxOutputStream

lzma_stream* lzstr = (lzma_stream*)m_lzStream;

if (!lzstr)
  m_lasterror = wxSTREAM_WRITE_ERROR;
if (!IsOk() || !bufsize)
  return 0;

lzma_ret ret_xz = LZMA_OK;
lzstr->next_in = (uint8_t*)buffer;
lzstr->avail_in = bufsize;

do
  {
    lzstr->next_out = (uint8_t*)m_pBuffer;
    lzstr->avail_out = OUT_BUF_MAX;

    ret_xz = lzma_code(lzstr, LZMA_RUN);

    if ((ret_xz != LZMA_OK) && (ret_xz != LZMA_STREAM_END))
      { m_lasterror = wxSTREAM_WRITE_ERROR;
        wxLogError(_("xz compression failure"));
        return false;
      }

    size_t out_len = OUT_BUF_MAX - lzstr->avail_out;
    m_parent_o_stream->Write(m_pBuffer, out_len);
    if (m_parent_o_stream->LastWrite() != out_len) 
      { m_lasterror = wxSTREAM_WRITE_ERROR;
        wxLogDebug(wxT("XzOutputStream: Error writing to underlying stream"));
        break;
      }
  }
   while (lzstr->avail_out == 0);

bufsize -= lzstr->avail_in;
m_nBufferPos += bufsize;
return bufsize;
}


bool XzOutputStream::Close() // Flushes any remaining compressed data
{
lzma_stream* lzstr = (lzma_stream*)m_lzStream;

if (!lzstr)
  m_lasterror = wxSTREAM_WRITE_ERROR;
if (!IsOk())
  return 0;

lzma_ret ret_xz = LZMA_OK;
lzstr->avail_in = 0;

do
  {
    lzstr->next_out = (uint8_t*)m_pBuffer;
    lzstr->avail_out = OUT_BUF_MAX;

    ret_xz = lzma_code(lzstr, LZMA_FINISH);

    if ((ret_xz != LZMA_OK) && (ret_xz != LZMA_STREAM_END))
      { m_lasterror = wxSTREAM_WRITE_ERROR;
        wxLogError(_("xz compression failure"));
        return false;
      }

    size_t out_len = OUT_BUF_MAX - lzstr->avail_out;
    m_parent_o_stream->Write(m_pBuffer, out_len);
    if (m_parent_o_stream->LastWrite() != out_len) 
      { m_lasterror = wxSTREAM_WRITE_ERROR;
        wxLogDebug(wxT("XzOutputStream: Error writing to underlying stream"));
        break;
      }
  }
   while (lzstr->avail_out == 0);

return wxFilterOutputStream::Close() && IsOk();
}

//-----------------------------------------------------------------------------------------


IMPLEMENT_DYNAMIC_CLASS(XzClassFactory, wxFilterClassFactory)

static XzClassFactory g_XzClassFactory;

XzClassFactory::XzClassFactory()
{
    if (this == &g_XzClassFactory)
        PushFront();
}

const wxChar * const *
XzClassFactory::GetProtocols(wxStreamProtocolType type) const
{
    static const wxChar *protos[] =     
        { wxT("xz"), NULL };
    static const wxChar *mimes[] = 
        { wxT("a\application/x-xz"), 
          wxT("application/x-xz-compressed-tar"), 
          NULL };
    static const wxChar *encs[] = 
        { wxT("xz"), NULL };
    static const wxChar *exts[] =    
        { wxT(".xz"), NULL };
    static const wxChar *empty[] =
        { NULL };

    switch (type) {
        case wxSTREAM_PROTOCOL: return protos;
        case wxSTREAM_MIMETYPE: return mimes;
        case wxSTREAM_ENCODING: return encs;
        case wxSTREAM_FILEEXT:  return exts;
        default:                return empty;
    }
}

#endif  // wxUSE_STREAMS
#endif  // ndef NO_LZMA_ARCHIVE_STREAMS
