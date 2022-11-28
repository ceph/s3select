
#pragma once

#if ! __has_include (<arrow/api.h>) || ! __has_include (<arrow/io/api.h>) || !__has_include (<parquet/arrow/reader.h>)
# undef _ARROW_EXIST 
#endif

#ifdef _ARROW_EXIST

#include <iostream>
#include <arrow/api.h>
#include <arrow/io/api.h>
#include <parquet/arrow/reader.h>
#include <parquet/arrow/writer.h>
#include <parquet/exception.h>
#include <set>
#include <parquet/column_reader.h>
#include <arrow/util/io_util.h>

#include <arrow/io/interfaces.h>
#include <utility>

#include <mutex>
#include <functional>

#include "internal_file_decryptor.h"
#include "encryption_internal.h"

#if ARROW_VERSION_MAJOR < 9                                                           
#define _ARROW_FD fd_
#define _ARROW_FD_TYPE int
#else
#define _ARROW_FD fd_.fd()
#define _ARROW_FD_TYPE arrow::internal::FileDescriptor
#endif

/******************************************/
/******************************************/
class optional_yield;
namespace s3selectEngine {
class rgw_s3select_api {

  // global object for setting interface between RGW and parquet-reader
  private:

  public:

  std::function<int(int64_t,int64_t,void*,optional_yield*)> range_req_fptr;
  std::function<size_t(void)> get_size_fptr;
  optional_yield *m_y;

  void set_range_req_api(std::function<int(int64_t,int64_t,void*,optional_yield*)> fp)
  {
    range_req_fptr = fp;
  }

  void set_get_size_api(std::function<size_t(void)> fp)
  {
    get_size_fptr = fp;
  }
};
}

/******************************************/
/******************************************/
/******************************************/

static constexpr uint8_t kParquetMagic[4] = {'P', 'A', 'R', '1'};
static constexpr uint8_t kParquetEMagic[4] = {'P', 'A', 'R', 'E'};
constexpr int kGcmTagLength = 16;

namespace arrow {
namespace io {
namespace internal {

ARROW_EXPORT void CloseFromDestructor(FileInterface* file);

// Validate a (offset, size) region (as given to ReadAt) against
// the file size.  Return the actual read size.
ARROW_EXPORT Result<int64_t> ValidateReadRange(int64_t offset, int64_t size,
                                               int64_t file_size);
// Validate a (offset, size) region (as given to WriteAt) against
// the file size.  Short writes are not allowed.
ARROW_EXPORT Status ValidateWriteRange(int64_t offset, int64_t size, int64_t file_size);

// Validate a (offset, size) region (as given to ReadAt or WriteAt), without
// knowing the file size.
ARROW_EXPORT Status ValidateRange(int64_t offset, int64_t size);

ARROW_EXPORT
std::vector<ReadRange> CoalesceReadRanges(std::vector<ReadRange> ranges,
                                          int64_t hole_size_limit,
                                          int64_t range_size_limit);

ARROW_EXPORT
::arrow::internal::ThreadPool* GetIOThreadPool();

}  // namespace internal
}  // namespace io
}


// RGWimpl and OSFile implements the access to storage objects, OSFile(filesystem files) RGWimpl( ceph S3 )
// ObjectInterface(temporary) is "empty base class" enables injections of  access function to storage-objects 
// ReadableFileImpl an implementation layer to ObjectInterface objects
// ReadableFile a layer which call to ReadableFileImpl, enable runtime switching between implementations 
// ParquetFileReader is the main interface (underline implementation is transparent to this layer) 
// 


namespace arrow {
class Buffer;
namespace io {

class ObjectInterface {

#define NOT_IMPLEMENTED {std::cout << "not implemented" << std::endl;}

//purpose: to implement the range-request from single object
public:
  ObjectInterface() : fd_(-1), is_open_(false), size_(-1), need_seeking_(false) {}

  virtual ~ObjectInterface(){}

  // Note: only one of the Open* methods below may be called on a given instance

  virtual Status OpenWritable(const std::string& path, bool truncate, bool append, bool write_only){return  Status::OK();}

  // This is different from OpenWritable(string, ...) in that it doesn't
  // truncate nor mandate a seekable file
  virtual Status OpenWritable(int fd){return  Status::OK();} 

  virtual Status OpenReadable(const std::string& path){return  Status::OK();}

  virtual Status OpenReadable(int fd){return  Status::OK();}

  virtual Status CheckClosed() const {return  Status::OK();}

  virtual Status Close(){return  Status::OK();} 

  virtual Result<int64_t> Read(int64_t nbytes, void* out){return Result<int64_t>(-1);}

  virtual Result<int64_t> ReadAt(int64_t position, int64_t nbytes, void* out){return Result<int64_t>(-1);}

  virtual Status Seek(int64_t pos){return  Status::OK();}

  virtual Result<int64_t> Tell() const {return Result<int64_t>(-1);}

  virtual Status Write(const void* data, int64_t length){return  Status::OK();}

  virtual int fd() const{return -1;}

  virtual bool is_open() const{return false;}

  virtual int64_t size() const{return -1;}

  virtual FileMode::type mode() const{return FileMode::READ;}

  #if 0
  std::mutex& lock(){}
  #endif

 protected:
  virtual Status SetFileName(const std::string& file_name){return  Status::OK();}

  virtual Status SetFileName(int fd){return  Status::OK();}

  virtual Status CheckPositioned(){return  Status::OK();}

  ::arrow::internal::PlatformFilename file_name_;

  std::mutex lock_;

  // File descriptor
  _ARROW_FD_TYPE fd_;

  FileMode::type mode_;

  bool is_open_;
  int64_t size_;
  // Whether ReadAt made the file position non-deterministic.
  std::atomic<bool> need_seeking_;

}; //ObjectInterface

} //namespace io
} //namespace arrow

namespace arrow {

using internal::IOErrorFromErrno;

namespace io {

class OSFile : public ObjectInterface {
 public:
  OSFile() : fd_(-1), is_open_(false), size_(-1), need_seeking_(false) {}

  ~OSFile() {}

  // Note: only one of the Open* methods below may be called on a given instance

  Status OpenWritable(const std::string& path, bool truncate, bool append,
                      bool write_only) override {
    RETURN_NOT_OK(SetFileName(path));

    ARROW_ASSIGN_OR_RAISE(fd_, ::arrow::internal::FileOpenWritable(file_name_, write_only,
                                                                   truncate, append));
    is_open_ = true;
    mode_ = write_only ? FileMode::WRITE : FileMode::READWRITE;

    if (!truncate) {
      ARROW_ASSIGN_OR_RAISE(size_, ::arrow::internal::FileGetSize(_ARROW_FD));
    } else {
      size_ = 0;
    }
    return Status::OK();
  }

  // This is different from OpenWritable(string, ...) in that it doesn't
  // truncate nor mandate a seekable file
  Status OpenWritable(int fd) override {
    auto result = ::arrow::internal::FileGetSize(fd);
    if (result.ok()) {
      size_ = *result;
    } else {
      // Non-seekable file
      size_ = -1;
    }
    RETURN_NOT_OK(SetFileName(fd));
    is_open_ = true;
    mode_ = FileMode::WRITE;
    #if ARROW_VERSION_MAJOR < 9
    fd_ = fd;
    #else
    fd_ = arrow::internal::FileDescriptor{fd};
    #endif
    return Status::OK();
  }

  Status OpenReadable(const std::string& path) override {
    RETURN_NOT_OK(SetFileName(path));

    ARROW_ASSIGN_OR_RAISE(fd_, ::arrow::internal::FileOpenReadable(file_name_));
    ARROW_ASSIGN_OR_RAISE(size_, ::arrow::internal::FileGetSize(_ARROW_FD));

    is_open_ = true;
    mode_ = FileMode::READ;
    return Status::OK();
  }

  Status OpenReadable(int fd) override {
    ARROW_ASSIGN_OR_RAISE(size_, ::arrow::internal::FileGetSize(fd));
    RETURN_NOT_OK(SetFileName(fd));
    is_open_ = true;
    mode_ = FileMode::READ;
    #if ARROW_VERSION_MAJOR < 9
    fd_ = fd;
    #else
    fd_ = arrow::internal::FileDescriptor{fd};
    #endif
    return Status::OK();
  }

  Status CheckClosed() const override {
    if (!is_open_) {
      return Status::Invalid("Invalid operation on closed file");
    }
    return Status::OK();
  }

  Status Close() override {
    if (is_open_) {
      // Even if closing fails, the fd will likely be closed (perhaps it's
      // already closed).
      is_open_ = false;
      #if ARROW_VERSION_MAJOR < 9
      int fd = fd_;
      fd_ = -1;
      RETURN_NOT_OK(::arrow::internal::FileClose(fd));
      #else
      RETURN_NOT_OK(fd_.Close());
      #endif
    }
    return Status::OK();
  }

  Result<int64_t> Read(int64_t nbytes, void* out) override {
    RETURN_NOT_OK(CheckClosed());
    RETURN_NOT_OK(CheckPositioned());
    return ::arrow::internal::FileRead(_ARROW_FD, reinterpret_cast<uint8_t*>(out), nbytes);
  }

  Result<int64_t> ReadAt(int64_t position, int64_t nbytes, void* out) override {
    RETURN_NOT_OK(CheckClosed());
    RETURN_NOT_OK(internal::ValidateRange(position, nbytes));
    // ReadAt() leaves the file position undefined, so require that we seek
    // before calling Read() or Write().
    need_seeking_.store(true);
    return ::arrow::internal::FileReadAt(_ARROW_FD, reinterpret_cast<uint8_t*>(out), position,
                                         nbytes);
  }

  Status Seek(int64_t pos) override {
    RETURN_NOT_OK(CheckClosed());
    if (pos < 0) {
      return Status::Invalid("Invalid position");
    }
    Status st = ::arrow::internal::FileSeek(_ARROW_FD, pos);
    if (st.ok()) {
      need_seeking_.store(false);
    }
    return st;
  }

  Result<int64_t> Tell() const override {
    RETURN_NOT_OK(CheckClosed());
    return ::arrow::internal::FileTell(_ARROW_FD);
  }

  Status Write(const void* data, int64_t length) override {
    RETURN_NOT_OK(CheckClosed());

    std::lock_guard<std::mutex> guard(lock_);
    RETURN_NOT_OK(CheckPositioned());
    if (length < 0) {
      return Status::IOError("Length must be non-negative");
    }
    return ::arrow::internal::FileWrite(_ARROW_FD, reinterpret_cast<const uint8_t*>(data),
                                        length);
  }

  int fd() const override { return _ARROW_FD; }

  bool is_open() const override { return is_open_; }

  int64_t size() const override { return size_; }

  FileMode::type mode() const override { return mode_; }

  std::mutex& lock() { return lock_; }

 protected:
  Status SetFileName(const std::string& file_name) override {
    return ::arrow::internal::PlatformFilename::FromString(file_name).Value(&file_name_);
  }

  Status SetFileName(int fd) override {
    std::stringstream ss;
    ss << "<fd " << fd << ">";
    return SetFileName(ss.str());
  }

  Status CheckPositioned() override {
    if (need_seeking_.load()) {
      return Status::Invalid(
          "Need seeking after ReadAt() before "
          "calling implicitly-positioned operation");
    }
    return Status::OK();
  }

  ::arrow::internal::PlatformFilename file_name_;

  std::mutex lock_;

  // File descriptor
  _ARROW_FD_TYPE fd_;

  FileMode::type mode_;

  bool is_open_;
  int64_t size_;
  // Whether ReadAt made the file position non-deterministic.
  std::atomic<bool> need_seeking_;
};
} // namespace io
} // namespace arrow

namespace arrow {
class Buffer;
namespace io {

class RGWimpl : public ObjectInterface {

//purpose: to implement the range-request from single object
public:
  RGWimpl(s3selectEngine::rgw_s3select_api* rgw) : fd_(-1), is_open_(false), size_(-1), need_seeking_(false),m_rgw_impl(rgw) {}

  ~RGWimpl(){}

#define NOT_IMPLEMENT { \
    std::stringstream ss; \
    ss << " method " << __FUNCTION__ << " is not implemented;"; \
    throw parquet::ParquetException(ss.str()); \
  }

  // Note: only one of the Open* methods below may be called on a given instance

  Status OpenWritable(const std::string& path, bool truncate, bool append, bool write_only) { NOT_IMPLEMENT;return Status::OK(); }

  // This is different from OpenWritable(string, ...) in that it doesn't
  // truncate nor mandate a seekable file
  Status OpenWritable(int fd) {NOT_IMPLEMENT;return Status::OK(); }

  Status OpenReadable(const std::string& path) {
    //RGW-implement 
    
    RETURN_NOT_OK(SetFileName(path));//TODO can skip that
    size_ = m_rgw_impl->get_size_fptr();

    is_open_ = true;
    mode_ = FileMode::READ;
    return Status::OK();
  }

  Status OpenReadable(int fd) {NOT_IMPLEMENT;return Status::OK(); }

  Status CheckClosed() const {
    //RGW-implement 
    if (!is_open_) {
      return Status::Invalid("Invalid operation on closed file");
    }
    return Status::OK();
  }

  Status Close() {
    //RGW-implement 
    if (is_open_) {
      // Even if closing fails, the fd will likely be closed (perhaps it's
      // already closed).
      is_open_ = false;
      //int fd = fd_;
      #if ARROW_VERSION_MAJOR < 9
      fd_ = -1;
      #else
      fd_.Close();
      #endif
      //RETURN_NOT_OK(::arrow::internal::FileClose(fd));
    }
    return Status::OK();
  }

  Result<int64_t> Read(int64_t nbytes, void* out) {
    NOT_IMPLEMENT;
    RETURN_NOT_OK(CheckClosed());
    RETURN_NOT_OK(CheckPositioned());
    return ::arrow::internal::FileRead(_ARROW_FD, reinterpret_cast<uint8_t*>(out), nbytes);
  }

  Result<int64_t> ReadAt(int64_t position, int64_t nbytes, void* out) {

     Result<int64_t> status =  m_rgw_impl->range_req_fptr(position,nbytes,out,m_rgw_impl->m_y);

     return status;
  }

  Status Seek(int64_t pos) {NOT_IMPLEMENT;return Status::OK(); }

  Result<int64_t> Tell() const {
    NOT_IMPLEMENT;
    return Result<int64_t>(0);
  }

  Status Write(const void* data, int64_t length) {
    NOT_IMPLEMENT;
    return Status::OK();
  }

  int fd() const { return _ARROW_FD; }

  bool is_open() const { return is_open_; }

  int64_t size() const { return size_; }

  FileMode::type mode() const { return mode_; }

  std::mutex& lock() { return lock_; } //TODO skip

 protected:
  Status SetFileName(const std::string& file_name) override {
    return ::arrow::internal::PlatformFilename::FromString(file_name).Value(&file_name_);
  }

  Status SetFileName(int fd) {NOT_IMPLEMENT; return Status::OK(); }

  Status CheckPositioned() {NOT_IMPLEMENT; return Status::OK(); }

  ::arrow::internal::PlatformFilename file_name_;

  std::mutex lock_;

  // File descriptor
  _ARROW_FD_TYPE fd_;

  FileMode::type mode_;

  bool is_open_;
  int64_t size_;
  // Whether ReadAt made the file position non-deterministic.
  std::atomic<bool> need_seeking_;

private:

  s3selectEngine::rgw_s3select_api* m_rgw_impl;
};

} //namespace io
} //namespace arrow

namespace arrow {

class Buffer;
class MemoryPool;
class Status;

namespace io {
namespace ceph {

/// \brief An operating system file open in read-only mode.
///
/// Reads through this implementation are unbuffered.  If many small reads
/// need to be issued, it is recommended to use a buffering layer for good
/// performance.
class ARROW_EXPORT ReadableFile
    : public internal::RandomAccessFileConcurrencyWrapper<ReadableFile> {
 public:
  ~ReadableFile() override;

  /// \brief Open a local file for reading
  /// \param[in] path with UTF8 encoding
  /// \param[in] pool a MemoryPool for memory allocations
  /// \return ReadableFile instance
  static Result<std::shared_ptr<ReadableFile>> Open(
      const std::string& path,s3selectEngine::rgw_s3select_api* rgw,MemoryPool* pool = default_memory_pool());

  /// \brief Open a local file for reading
  /// \param[in] fd file descriptor
  /// \param[in] pool a MemoryPool for memory allocations
  /// \return ReadableFile instance
  ///
  /// The file descriptor becomes owned by the ReadableFile, and will be closed
  /// on Close() or destruction.
  static Result<std::shared_ptr<ReadableFile>> Open(
      int fd, MemoryPool* pool = default_memory_pool());

  bool closed() const override;

  int file_descriptor() const;

  Status WillNeed(const std::vector<ReadRange>& ranges) override;

 private:
  friend RandomAccessFileConcurrencyWrapper<ReadableFile>;

  explicit ReadableFile(MemoryPool* pool,s3selectEngine::rgw_s3select_api* rgw);

  Status DoClose();
  Result<int64_t> DoTell() const;
  Result<int64_t> DoRead(int64_t nbytes, void* buffer);
  Result<std::shared_ptr<Buffer>> DoRead(int64_t nbytes);

  /// \brief Thread-safe implementation of ReadAt
  Result<int64_t> DoReadAt(int64_t position, int64_t nbytes, void* out);

  /// \brief Thread-safe implementation of ReadAt
  Result<std::shared_ptr<Buffer>> DoReadAt(int64_t position, int64_t nbytes);

  Result<int64_t> DoGetSize();
  Status DoSeek(int64_t position);

  class ARROW_NO_EXPORT ReadableFileImpl;
  std::unique_ptr<ReadableFileImpl> impl_;
};


} // namespace ceph
} // namespace io
} // namespace arrow

// ----------------------------------------------------------------------
// ReadableFileImpl implementation

namespace arrow {
namespace io {
namespace ceph {

class ReadableFile::ReadableFileImpl : public ObjectInterface {
 public:
 
  ~ReadableFileImpl()
  {
    if(IMPL != nullptr)
    {
      delete IMPL;
    }
  }

#ifdef CEPH_USE_FS
  explicit ReadableFileImpl(MemoryPool* pool) :  pool_(pool) {IMPL=new OSFile();}
#endif
  explicit ReadableFileImpl(MemoryPool* pool,s3selectEngine::rgw_s3select_api* rgw) :  pool_(pool) {IMPL=new RGWimpl(rgw);}

  Status Open(const std::string& path) { return IMPL->OpenReadable(path); }

  Status Open(int fd) { return IMPL->OpenReadable(fd); }

  Result<std::shared_ptr<Buffer>> ReadBuffer(int64_t nbytes) {
    ARROW_ASSIGN_OR_RAISE(auto buffer, AllocateResizableBuffer(nbytes, pool_));

    ARROW_ASSIGN_OR_RAISE(int64_t bytes_read, IMPL->Read(nbytes, buffer->mutable_data()));
    if (bytes_read < nbytes) {
      RETURN_NOT_OK(buffer->Resize(bytes_read));
      buffer->ZeroPadding();
    }
    return buffer;
  }

  Result<std::shared_ptr<Buffer>> ReadBufferAt(int64_t position, int64_t nbytes) {
    ARROW_ASSIGN_OR_RAISE(auto buffer, AllocateResizableBuffer(nbytes, pool_));

    ARROW_ASSIGN_OR_RAISE(int64_t bytes_read,
                          IMPL->ReadAt(position, nbytes, buffer->mutable_data()));
    if (bytes_read < nbytes) {
      RETURN_NOT_OK(buffer->Resize(bytes_read));
      buffer->ZeroPadding();
    }
    return buffer;
  }

  Status WillNeed(const std::vector<ReadRange>& ranges) {
    RETURN_NOT_OK(CheckClosed());
    for (const auto& range : ranges) {
      RETURN_NOT_OK(internal::ValidateRange(range.offset, range.length));
#if defined(POSIX_FADV_WILLNEED)
      if (posix_fadvise(_ARROW_FD, range.offset, range.length, POSIX_FADV_WILLNEED)) {
        return IOErrorFromErrno(errno, "posix_fadvise failed");
      }
#elif defined(F_RDADVISE)  // macOS, BSD?
      struct {
        off_t ra_offset;
        int ra_count;
      } radvisory{range.offset, static_cast<int>(range.length)};
      if (radvisory.ra_count > 0 && fcntl(_ARROW_FD, F_RDADVISE, &radvisory) == -1) {
        return IOErrorFromErrno(errno, "fcntl(fd, F_RDADVISE, ...) failed");
      }
#endif
    }
    return Status::OK();
  }

  ObjectInterface *IMPL;//TODO to declare in ObjectInterface 

 private:
 
  MemoryPool* pool_;
  
};

// ReadableFile implemmetation 
ReadableFile::ReadableFile(MemoryPool* pool,s3selectEngine::rgw_s3select_api* rgw) { impl_.reset(new ReadableFileImpl(pool,rgw)); }

ReadableFile::~ReadableFile() { internal::CloseFromDestructor(this); }

Result<std::shared_ptr<ReadableFile>> ReadableFile::Open(const std::string& path,
                                                         s3selectEngine::rgw_s3select_api* rgw,
                                                         MemoryPool* pool
                                                         ) {
  auto file = std::shared_ptr<ReadableFile>(new ReadableFile(pool,rgw));
  RETURN_NOT_OK(file->impl_->Open(path));
  return file;
}

Result<std::shared_ptr<ReadableFile>> ReadableFile::Open(int fd, MemoryPool* pool) {
  NOT_IMPLEMENT;
  auto file = std::shared_ptr<ReadableFile>(new ReadableFile(pool,0));
  RETURN_NOT_OK(file->impl_->Open(fd));
  return file;
}

Status ReadableFile::DoClose() { return impl_->Close(); }

bool ReadableFile::closed() const { return !impl_->is_open(); }

Status ReadableFile::WillNeed(const std::vector<ReadRange>& ranges) {
  return impl_->WillNeed(ranges);
}

Result<int64_t> ReadableFile::DoTell() const { return impl_->Tell(); }

Result<int64_t> ReadableFile::DoRead(int64_t nbytes, void* out) {
  return impl_->IMPL->Read(nbytes, out);
}

Result<int64_t> ReadableFile::DoReadAt(int64_t position, int64_t nbytes, void* out) {
  return impl_->IMPL->ReadAt(position, nbytes, out);
}

Result<std::shared_ptr<Buffer>> ReadableFile::DoReadAt(int64_t position, int64_t nbytes) {
  return impl_->ReadBufferAt(position, nbytes);
}

Result<std::shared_ptr<Buffer>> ReadableFile::DoRead(int64_t nbytes) {
  return impl_->ReadBuffer(nbytes);
}

Result<int64_t> ReadableFile::DoGetSize() { return impl_->IMPL->size(); }

Status ReadableFile::DoSeek(int64_t pos) { return impl_->IMPL->Seek(pos); }

int ReadableFile::file_descriptor() const { return impl_->IMPL->fd(); }

} // namepace ceph
} // namespace io
} // namespace arrow


namespace parquet {

class ColumnReader;
class FileMetaData;
class PageReader;
class RandomAccessSource;
class RowGroupMetaData;

namespace ceph {
class PARQUET_EXPORT RowGroupReader {
 public:
  // Forward declare a virtual class 'Contents' to aid dependency injection and more
  // easily create test fixtures
  // An implementation of the Contents class is defined in the .cc file
  struct Contents {
    virtual ~Contents() {}
    virtual std::unique_ptr<PageReader> GetColumnPageReader(int i) = 0;
    virtual const RowGroupMetaData* metadata() const = 0;
    virtual const ReaderProperties* properties() const = 0;
  };

  explicit RowGroupReader(std::unique_ptr<Contents> contents);

  // Returns the rowgroup metadata
  const RowGroupMetaData* metadata() const;

  // Construct a ColumnReader for the indicated row group-relative
  // column. Ownership is shared with the RowGroupReader.
  std::shared_ptr<ColumnReader> Column(int i);

  std::unique_ptr<PageReader> GetColumnPageReader(int i);

 private:
  // Holds a pointer to an instance of Contents implementation
  std::unique_ptr<Contents> contents_;
};

class PARQUET_EXPORT ParquetFileReader {
 public:
  // Declare a virtual class 'Contents' to aid dependency injection and more
  // easily create test fixtures
  // An implementation of the Contents class is defined in the .cc file
  struct PARQUET_EXPORT Contents {
    static std::unique_ptr<Contents> Open(
        std::shared_ptr<::arrow::io::RandomAccessFile> source,
        const ReaderProperties& props = default_reader_properties(),
        std::shared_ptr<FileMetaData> metadata = NULLPTR);

    virtual ~Contents() = default;
    // Perform any cleanup associated with the file contents
    virtual void Close() = 0;
    virtual std::shared_ptr<RowGroupReader> GetRowGroup(int i) = 0;
    virtual std::shared_ptr<FileMetaData> metadata() const = 0;
  };

  ParquetFileReader();
  ~ParquetFileReader();

  // Create a reader from some implementation of parquet-cpp's generic file
  // input interface
  //
  // If you cannot provide exclusive access to your file resource, create a
  // subclass of RandomAccessSource that wraps the shared resource
  ARROW_DEPRECATED("Use arrow::io::RandomAccessFile version")
  static std::unique_ptr<ParquetFileReader> Open(
      std::unique_ptr<RandomAccessSource> source,
      const ReaderProperties& props = default_reader_properties(),
      std::shared_ptr<FileMetaData> metadata = NULLPTR);

  // Create a file reader instance from an Arrow file object. Thread-safety is
  // the responsibility of the file implementation
  static std::unique_ptr<ParquetFileReader> Open(
      std::shared_ptr<::arrow::io::RandomAccessFile> source,
      const ReaderProperties& props = default_reader_properties(),
      std::shared_ptr<FileMetaData> metadata = NULLPTR);

  // API Convenience to open a serialized Parquet file on disk, using Arrow IO
  // interfaces.
  static std::unique_ptr<ParquetFileReader> OpenFile(
      const std::string& path,s3selectEngine::rgw_s3select_api* rgw, bool memory_map = true,
      const ReaderProperties& props = default_reader_properties(),
      std::shared_ptr<FileMetaData> metadata = NULLPTR
      );

  void Open(std::unique_ptr<Contents> contents);
  void Close();

  // The RowGroupReader is owned by the FileReader
  std::shared_ptr<RowGroupReader> RowGroup(int i);

  // Returns the file metadata. Only one instance is ever created
  std::shared_ptr<FileMetaData> metadata() const;

  /// Pre-buffer the specified column indices in all row groups.
  ///
  /// Readers can optionally call this to cache the necessary slices
  /// of the file in-memory before deserialization. Arrow readers can
  /// automatically do this via an option. This is intended to
  /// increase performance when reading from high-latency filesystems
  /// (e.g. Amazon S3).
  ///
  /// After calling this, creating readers for row groups/column
  /// indices that were not buffered may fail. Creating multiple
  /// readers for the a subset of the buffered regions is
  /// acceptable. This may be called again to buffer a different set
  /// of row groups/columns.
  ///
  /// If memory usage is a concern, note that data will remain
  /// buffered in memory until either \a PreBuffer() is called again,
  /// or the reader itself is destructed. Reading - and buffering -
  /// only one row group at a time may be useful.
  void PreBuffer(const std::vector<int>& row_groups,
                 const std::vector<int>& column_indices,
                 const ::arrow::io::IOContext& ctx,
                 const ::arrow::io::CacheOptions& options);

 private:
  // Holds a pointer to an instance of Contents implementation
  std::unique_ptr<Contents> contents_;
};

// Read only Parquet file metadata
std::shared_ptr<FileMetaData> PARQUET_EXPORT
ReadMetaData(const std::shared_ptr<::arrow::io::RandomAccessFile>& source);

/// \brief Scan all values in file. Useful for performance testing
/// \param[in] columns the column numbers to scan. If empty scans all
/// \param[in] column_batch_size number of values to read at a time when scanning column
/// \param[in] reader a ParquetFileReader instance
/// \return number of semantic rows in file
PARQUET_EXPORT
int64_t ScanFileContents(std::vector<int> columns, const int32_t column_batch_size,
                         ParquetFileReader* reader);

}//namespace ceph
}//namespace parquet


namespace parquet {

namespace ceph {

// PARQUET-978: Minimize footer reads by reading 64 KB from the end of the file
static constexpr int64_t kDefaultFooterReadSize = 64 * 1024;
static constexpr uint32_t kFooterSize = 8;

// For PARQUET-816
static constexpr int64_t kMaxDictHeaderSize = 100;

// ----------------------------------------------------------------------
// RowGroupReader public API

RowGroupReader::RowGroupReader(std::unique_ptr<Contents> contents)
    : contents_(std::move(contents)) {}

std::shared_ptr<ColumnReader> RowGroupReader::Column(int i) {
  if (i >= metadata()->num_columns()) {
    std::stringstream ss;
    ss << "Trying to read column index " << i << " but row group metadata has only "
       << metadata()->num_columns() << " columns";
    throw ParquetException(ss.str());
  }
  const ColumnDescriptor* descr = metadata()->schema()->Column(i);

  std::unique_ptr<PageReader> page_reader = contents_->GetColumnPageReader(i);
  return ColumnReader::Make(
      descr, std::move(page_reader),
      const_cast<ReaderProperties*>(contents_->properties())->memory_pool());
}

std::unique_ptr<PageReader> RowGroupReader::GetColumnPageReader(int i) {
  if (i >= metadata()->num_columns()) {
    std::stringstream ss;
    ss << "Trying to read column index " << i << " but row group metadata has only "
       << metadata()->num_columns() << " columns";
    throw ParquetException(ss.str());
  }
  return contents_->GetColumnPageReader(i);
}

// Returns the rowgroup metadata
const RowGroupMetaData* RowGroupReader::metadata() const { return contents_->metadata(); }

/// Compute the section of the file that should be read for the given
/// row group and column chunk.
::arrow::io::ReadRange ComputeColumnChunkRange(FileMetaData* file_metadata,
                                             int64_t source_size, int row_group_index,
                                             int column_index) {
  auto row_group_metadata = file_metadata->RowGroup(row_group_index);
  auto column_metadata = row_group_metadata->ColumnChunk(column_index);

  int64_t col_start = column_metadata->data_page_offset();
  if (column_metadata->has_dictionary_page() &&
      column_metadata->dictionary_page_offset() > 0 &&
      col_start > column_metadata->dictionary_page_offset()) {
    col_start = column_metadata->dictionary_page_offset();
  }

  int64_t col_length = column_metadata->total_compressed_size();
  // PARQUET-816 workaround for old files created by older parquet-mr
  const ApplicationVersion& version = file_metadata->writer_version();
  if (version.VersionLt(ApplicationVersion::PARQUET_816_FIXED_VERSION())) {
    // The Parquet MR writer had a bug in 1.2.8 and below where it didn't include the
    // dictionary page header size in total_compressed_size and total_uncompressed_size
    // (see IMPALA-694). We add padding to compensate.
    int64_t bytes_remaining = source_size - (col_start + col_length);
    int64_t padding = std::min<int64_t>(kMaxDictHeaderSize, bytes_remaining);
    col_length += padding;
  }

  return {col_start, col_length};
}

// RowGroupReader::Contents implementation for the Parquet file specification
class SerializedRowGroup : public RowGroupReader::Contents {
 public:
  SerializedRowGroup(std::shared_ptr<ArrowInputFile> source,
                     std::shared_ptr<::arrow::io::internal::ReadRangeCache> cached_source,
                     int64_t source_size, FileMetaData* file_metadata,
                     int row_group_number, const ReaderProperties& props,
                     std::shared_ptr<parquet::InternalFileDecryptor> file_decryptor = nullptr)
      : source_(std::move(source)),
        cached_source_(std::move(cached_source)),
        source_size_(source_size),
        file_metadata_(file_metadata),
        properties_(props),
        row_group_ordinal_(row_group_number),
        file_decryptor_(file_decryptor) {
    row_group_metadata_ = file_metadata->RowGroup(row_group_number);
  }

  const RowGroupMetaData* metadata() const override { return row_group_metadata_.get(); }

  const ReaderProperties* properties() const override { return &properties_; }

  std::unique_ptr<PageReader> GetColumnPageReader(int i) override {
    // Read column chunk from the file
    auto col = row_group_metadata_->ColumnChunk(i);

    ::arrow::io::ReadRange col_range =
        ComputeColumnChunkRange(file_metadata_, source_size_, row_group_ordinal_, i);
    std::shared_ptr<ArrowInputStream> stream;
    if (cached_source_) {
      // PARQUET-1698: if read coalescing is enabled, read from pre-buffered
      // segments.
      PARQUET_ASSIGN_OR_THROW(auto buffer, cached_source_->Read(col_range));
      stream = std::make_shared<::arrow::io::BufferReader>(buffer);
    } else {
      stream = properties_.GetStream(source_, col_range.offset, col_range.length);
    }

    std::unique_ptr<ColumnCryptoMetaData> crypto_metadata = col->crypto_metadata();

    // Column is encrypted only if crypto_metadata exists.
    if (!crypto_metadata) {
      return PageReader::Open(stream, col->num_values(), col->compression(),
                              properties_.memory_pool());
    }

    if (file_decryptor_ == nullptr) {
      throw ParquetException("RowGroup is noted as encrypted but no file decryptor");
    }

    constexpr auto kEncryptedRowGroupsLimit = 32767;
    if (i > kEncryptedRowGroupsLimit) {
      throw ParquetException("Encrypted files cannot contain more than 32767 row groups");
    }

    // The column is encrypted
    std::shared_ptr<::parquet::Decryptor> meta_decryptor;
    std::shared_ptr<Decryptor> data_decryptor;
    // The column is encrypted with footer key
    if (crypto_metadata->encrypted_with_footer_key()) {
      meta_decryptor = file_decryptor_->GetFooterDecryptorForColumnMeta();
      data_decryptor = file_decryptor_->GetFooterDecryptorForColumnData();
 
      CryptoContext ctx(col->has_dictionary_page(), row_group_ordinal_,
                        static_cast<int16_t>(i), meta_decryptor, data_decryptor);
      return PageReader::Open(stream, col->num_values(), col->compression(),
      #if ARROW_VERSION_MAJOR > 8
                              false,
      #endif
                              properties_.memory_pool(), &ctx);
    }

    // The column is encrypted with its own key
    std::string column_key_metadata = crypto_metadata->key_metadata();
    const std::string column_path = crypto_metadata->path_in_schema()->ToDotString();

    meta_decryptor =
        file_decryptor_->GetColumnMetaDecryptor(column_path, column_key_metadata);
    data_decryptor =
        file_decryptor_->GetColumnDataDecryptor(column_path, column_key_metadata);

    CryptoContext ctx(col->has_dictionary_page(), row_group_ordinal_,
                      static_cast<int16_t>(i), meta_decryptor, data_decryptor);
    return PageReader::Open(stream, col->num_values(), col->compression(),
    #if ARROW_VERSION_MAJOR > 8
                            false,
    #endif
                            properties_.memory_pool(), &ctx);
  }

 private:
  std::shared_ptr<ArrowInputFile> source_;
  // Will be nullptr if PreBuffer() is not called.
  std::shared_ptr<::arrow::io::internal::ReadRangeCache> cached_source_;
  int64_t source_size_;
  FileMetaData* file_metadata_;
  std::unique_ptr<RowGroupMetaData> row_group_metadata_;
  ReaderProperties properties_;
  int row_group_ordinal_;
  std::shared_ptr<InternalFileDecryptor> file_decryptor_;
};

// ----------------------------------------------------------------------
// SerializedFile: An implementation of ParquetFileReader::Contents that deals
// with the Parquet file structure, Thrift deserialization, and other internal
// matters

// This class takes ownership of the provided data source
class SerializedFile : public ParquetFileReader::Contents {
 public:
  SerializedFile(std::shared_ptr<ArrowInputFile> source,
                 const ReaderProperties& props = default_reader_properties())
      : source_(std::move(source)), properties_(props) {
    PARQUET_ASSIGN_OR_THROW(source_size_, source_->GetSize());
  }

  ~SerializedFile() override {
    try {
      Close();
    } catch (...) {
    }
  }

  void Close() override {
    if (file_decryptor_) file_decryptor_->WipeOutDecryptionKeys();
  }

  std::shared_ptr<RowGroupReader> GetRowGroup(int i) override {
    std::unique_ptr<SerializedRowGroup> contents(
        new SerializedRowGroup(source_, cached_source_, source_size_,
                               file_metadata_.get(), i, properties_, file_decryptor_));
    return std::make_shared<RowGroupReader>(std::move(contents));
  }

  std::shared_ptr<FileMetaData> metadata() const override { return file_metadata_; }

  void set_metadata(std::shared_ptr<FileMetaData> metadata) {
    file_metadata_ = std::move(metadata);
  }

  void PreBuffer(const std::vector<int>& row_groups,
                 const std::vector<int>& column_indices,
                 const ::arrow::io::IOContext& ctx,
                 const ::arrow::io::CacheOptions& options) {
    cached_source_ =
        std::make_shared<::arrow::io::internal::ReadRangeCache>(source_, ctx, options);
    //std::vector<arrow::io::ReadRange> ranges;
    std::vector<::arrow::io::ReadRange> ranges;
    for (int row : row_groups) {
      for (int col : column_indices) {
        ranges.push_back(
            ComputeColumnChunkRange(file_metadata_.get(), source_size_, row, col));
      }
    }
    PARQUET_THROW_NOT_OK(cached_source_->Cache(ranges));
  }

  void ParseMetaData() {
    if (source_size_ == 0) {
      throw ParquetInvalidOrCorruptedFileException("Parquet file size is 0 bytes");
    } else if (source_size_ < kFooterSize) {
      throw ParquetInvalidOrCorruptedFileException(
          "Parquet file size is ", source_size_,
          " bytes, smaller than the minimum file footer (", kFooterSize, " bytes)");
    }

    int64_t footer_read_size = std::min(source_size_, kDefaultFooterReadSize);
    PARQUET_ASSIGN_OR_THROW(
        auto footer_buffer,
        source_->ReadAt(source_size_ - footer_read_size, footer_read_size));

    // Check if all bytes are read. Check if last 4 bytes read have the magic bits
    if (footer_buffer->size() != footer_read_size ||
        (memcmp(footer_buffer->data() + footer_read_size - 4, kParquetMagic, 4) != 0 &&
         memcmp(footer_buffer->data() + footer_read_size - 4, kParquetEMagic, 4) != 0)) {
      throw ParquetInvalidOrCorruptedFileException(
          "Parquet magic bytes not found in footer. Either the file is corrupted or this "
          "is not a parquet file.");
    }

    if (memcmp(footer_buffer->data() + footer_read_size - 4, kParquetEMagic, 4) == 0) {
      // Encrypted file with Encrypted footer.
      ParseMetaDataOfEncryptedFileWithEncryptedFooter(footer_buffer, footer_read_size);
      return;
    }

    // No encryption or encryption with plaintext footer mode.
    std::shared_ptr<Buffer> metadata_buffer;
    uint32_t metadata_len, read_metadata_len;
    ParseUnencryptedFileMetadata(footer_buffer, footer_read_size, &metadata_buffer,
                                 &metadata_len, &read_metadata_len);

    auto file_decryption_properties = properties_.file_decryption_properties().get();
    if (!file_metadata_->is_encryption_algorithm_set()) {  // Non encrypted file.
      if (file_decryption_properties != nullptr) {
        if (!file_decryption_properties->plaintext_files_allowed()) {
          throw ParquetException("Applying decryption properties on plaintext file");
        }
      }
    } else {
      // Encrypted file with plaintext footer mode.
      ParseMetaDataOfEncryptedFileWithPlaintextFooter(
          file_decryption_properties, metadata_buffer, metadata_len, read_metadata_len);
    }
  }

 private:
  std::shared_ptr<ArrowInputFile> source_;
  std::shared_ptr<::arrow::io::internal::ReadRangeCache> cached_source_;
  int64_t source_size_;
  std::shared_ptr<FileMetaData> file_metadata_;
  ReaderProperties properties_;

  std::shared_ptr<::parquet::InternalFileDecryptor> file_decryptor_;

  void ParseUnencryptedFileMetadata(const std::shared_ptr<Buffer>& footer_buffer,
                                    int64_t footer_read_size,
                                    std::shared_ptr<Buffer>* metadata_buffer,
                                    uint32_t* metadata_len, uint32_t* read_metadata_len);

  std::string HandleAadPrefix(FileDecryptionProperties* file_decryption_properties,
                              EncryptionAlgorithm& algo);

  void ParseMetaDataOfEncryptedFileWithPlaintextFooter(
      FileDecryptionProperties* file_decryption_properties,
      const std::shared_ptr<Buffer>& metadata_buffer, uint32_t metadata_len,
      uint32_t read_metadata_len);

  void ParseMetaDataOfEncryptedFileWithEncryptedFooter(
      const std::shared_ptr<Buffer>& footer_buffer, int64_t footer_read_size);
};

void SerializedFile::ParseUnencryptedFileMetadata(
    const std::shared_ptr<Buffer>& footer_buffer, int64_t footer_read_size,
    std::shared_ptr<Buffer>* metadata_buffer, uint32_t* metadata_len,
    uint32_t* read_metadata_len) {
  *metadata_len = ::arrow::util::SafeLoadAs<uint32_t>(
      reinterpret_cast<const uint8_t*>(footer_buffer->data()) + footer_read_size -
      kFooterSize);
  int64_t metadata_start = source_size_ - kFooterSize - *metadata_len;
  if (*metadata_len > source_size_ - kFooterSize) {
    throw ParquetInvalidOrCorruptedFileException(
        "Parquet file size is ", source_size_,
        " bytes, smaller than the size reported by metadata (", metadata_len, "bytes)");
  }

  // Check if the footer_buffer contains the entire metadata
  if (footer_read_size >= (*metadata_len + kFooterSize)) {
    *metadata_buffer = SliceBuffer(
        footer_buffer, footer_read_size - *metadata_len - kFooterSize, *metadata_len);
  } else {
    PARQUET_ASSIGN_OR_THROW(*metadata_buffer,
                            source_->ReadAt(metadata_start, *metadata_len));
    if ((*metadata_buffer)->size() != *metadata_len) {
      throw ParquetException("Failed reading metadata buffer (requested " +
                             std::to_string(*metadata_len) + " bytes but got " +
                             std::to_string((*metadata_buffer)->size()) + " bytes)");
    }
  }

  *read_metadata_len = *metadata_len;
  file_metadata_ = FileMetaData::Make((*metadata_buffer)->data(), read_metadata_len);
}

void SerializedFile::ParseMetaDataOfEncryptedFileWithEncryptedFooter(
    const std::shared_ptr<Buffer>& footer_buffer, int64_t footer_read_size) {
  // encryption with encrypted footer
  // both metadata & crypto metadata length
  uint32_t footer_len = ::arrow::util::SafeLoadAs<uint32_t>(
      reinterpret_cast<const uint8_t*>(footer_buffer->data()) + footer_read_size -
      kFooterSize);
  int64_t crypto_metadata_start = source_size_ - kFooterSize - footer_len;
  if (kFooterSize + footer_len > source_size_) {
    throw ParquetInvalidOrCorruptedFileException(
        "Parquet file size is ", source_size_,
        " bytes, smaller than the size reported by footer's (", footer_len, "bytes)");
  }
  std::shared_ptr<Buffer> crypto_metadata_buffer;
  // Check if the footer_buffer contains the entire metadata
  if (footer_read_size >= (footer_len + kFooterSize)) {
    crypto_metadata_buffer = SliceBuffer(
        footer_buffer, footer_read_size - footer_len - kFooterSize, footer_len);
  } else {
    PARQUET_ASSIGN_OR_THROW(crypto_metadata_buffer,
                            source_->ReadAt(crypto_metadata_start, footer_len));
    if (crypto_metadata_buffer->size() != footer_len) {
      throw ParquetException("Failed reading encrypted metadata buffer (requested " +
                             std::to_string(footer_len) + " bytes but got " +
                             std::to_string(crypto_metadata_buffer->size()) + " bytes)");
    }
  }
  auto file_decryption_properties = properties_.file_decryption_properties().get();
  if (file_decryption_properties == nullptr) {
    throw ParquetException(
        "Could not read encrypted metadata, no decryption found in reader's properties");
  }
  uint32_t crypto_metadata_len = footer_len;
  std::shared_ptr<FileCryptoMetaData> file_crypto_metadata =
      FileCryptoMetaData::Make(crypto_metadata_buffer->data(), &crypto_metadata_len);
  // Handle AAD prefix
  EncryptionAlgorithm algo = file_crypto_metadata->encryption_algorithm();
  std::string file_aad = HandleAadPrefix(file_decryption_properties, algo);
  file_decryptor_ = std::make_shared<::parquet::InternalFileDecryptor>(
      file_decryption_properties, file_aad, algo.algorithm,
      file_crypto_metadata->key_metadata(), properties_.memory_pool());

  int64_t metadata_offset = source_size_ - kFooterSize - footer_len + crypto_metadata_len;
  uint32_t metadata_len = footer_len - crypto_metadata_len;
  PARQUET_ASSIGN_OR_THROW(auto metadata_buffer,
                          source_->ReadAt(metadata_offset, metadata_len));
  if (metadata_buffer->size() != metadata_len) {
    throw ParquetException("Failed reading metadata buffer (requested " +
                           std::to_string(metadata_len) + " bytes but got " +
                           std::to_string(metadata_buffer->size()) + " bytes)");
  }

  file_metadata_ =
      FileMetaData::Make(metadata_buffer->data(), &metadata_len, default_reader_properties(), file_decryptor_);
}

void SerializedFile::ParseMetaDataOfEncryptedFileWithPlaintextFooter(
    FileDecryptionProperties* file_decryption_properties,
    const std::shared_ptr<Buffer>& metadata_buffer, uint32_t metadata_len,
    uint32_t read_metadata_len) {
  // Providing decryption properties in plaintext footer mode is not mandatory, for
  // example when reading by legacy reader.
  if (file_decryption_properties != nullptr) {
    EncryptionAlgorithm algo = file_metadata_->encryption_algorithm();
    // Handle AAD prefix
    std::string file_aad = HandleAadPrefix(file_decryption_properties, algo);
    file_decryptor_ = std::make_shared<::parquet::InternalFileDecryptor>(
        file_decryption_properties, file_aad, algo.algorithm,
        file_metadata_->footer_signing_key_metadata(), properties_.memory_pool());
    // set the InternalFileDecryptor in the metadata as well, as it's used
    // for signature verification and for ColumnChunkMetaData creation.
#if GAL_set_file_decryptor_declare_private
    file_metadata_->set_file_decryptor(file_decryptor_);
#endif
    if (file_decryption_properties->check_plaintext_footer_integrity()) {
      if (metadata_len - read_metadata_len !=
          (parquet::encryption::kGcmTagLength + parquet::encryption::kNonceLength)) {
        throw ParquetInvalidOrCorruptedFileException(
            "Failed reading metadata for encryption signature (requested ",
            parquet::encryption::kGcmTagLength + parquet::encryption::kNonceLength,
            " bytes but have ", metadata_len - read_metadata_len, " bytes)");
      }

      if (!file_metadata_->VerifySignature(metadata_buffer->data() + read_metadata_len)) {
        throw ParquetInvalidOrCorruptedFileException(
            "Parquet crypto signature verification failed");
      }
    }
  }
}

std::string SerializedFile::HandleAadPrefix(
    FileDecryptionProperties* file_decryption_properties, EncryptionAlgorithm& algo) {
  std::string aad_prefix_in_properties = file_decryption_properties->aad_prefix();
  std::string aad_prefix = aad_prefix_in_properties;
  bool file_has_aad_prefix = algo.aad.aad_prefix.empty() ? false : true;
  std::string aad_prefix_in_file = algo.aad.aad_prefix;

  if (algo.aad.supply_aad_prefix && aad_prefix_in_properties.empty()) {
    throw ParquetException(
        "AAD prefix used for file encryption, "
        "but not stored in file and not supplied "
        "in decryption properties");
  }

  if (file_has_aad_prefix) {
    if (!aad_prefix_in_properties.empty()) {
      if (aad_prefix_in_properties.compare(aad_prefix_in_file) != 0) {
        throw ParquetException(
            "AAD Prefix in file and in properties "
            "is not the same");
      }
    }
    aad_prefix = aad_prefix_in_file;
    std::shared_ptr<AADPrefixVerifier> aad_prefix_verifier =
        file_decryption_properties->aad_prefix_verifier();
    if (aad_prefix_verifier != nullptr) aad_prefix_verifier->Verify(aad_prefix);
  } else {
    if (!algo.aad.supply_aad_prefix && !aad_prefix_in_properties.empty()) {
      throw ParquetException(
          "AAD Prefix set in decryption properties, but was not used "
          "for file encryption");
    }
    std::shared_ptr<AADPrefixVerifier> aad_prefix_verifier =
        file_decryption_properties->aad_prefix_verifier();
    if (aad_prefix_verifier != nullptr) {
      throw ParquetException(
          "AAD Prefix Verifier is set, but AAD Prefix not found in file");
    }
  }
  return aad_prefix + algo.aad.aad_file_unique;
}

// ----------------------------------------------------------------------
// ParquetFileReader public API

ParquetFileReader::ParquetFileReader() {}

ParquetFileReader::~ParquetFileReader() {
  try {
    Close();
  } catch (...) {
  }
}

// Open the file. If no metadata is passed, it is parsed from the footer of
// the file
std::unique_ptr<ParquetFileReader::Contents> ParquetFileReader::Contents::Open(
    std::shared_ptr<ArrowInputFile> source, const ReaderProperties& props,
    std::shared_ptr<FileMetaData> metadata) {
  std::unique_ptr<ParquetFileReader::Contents> result(
      new SerializedFile(std::move(source), props));

  // Access private methods here, but otherwise unavailable
  SerializedFile* file = static_cast<SerializedFile*>(result.get());

  if (metadata == nullptr) {
    // Validates magic bytes, parses metadata, and initializes the SchemaDescriptor
    file->ParseMetaData();
  } else {
    file->set_metadata(std::move(metadata));
  }

  return result;
}

std::unique_ptr<ParquetFileReader> ParquetFileReader::Open(
    std::shared_ptr<::arrow::io::RandomAccessFile> source, const ReaderProperties& props,
    std::shared_ptr<FileMetaData> metadata) {
  auto contents = SerializedFile::Open(std::move(source), props, std::move(metadata));
  std::unique_ptr<ParquetFileReader> result(new ParquetFileReader());
  result->Open(std::move(contents));
  return result;
}

#if GAL_NOT_IMPLEMENTED
std::unique_ptr<ParquetFileReader> ParquetFileReader::Open(
    std::unique_ptr<RandomAccessSource> source, const ReaderProperties& props,
    std::shared_ptr<FileMetaData> metadata) {
  auto wrapper = std::make_shared<ParquetInputWrapper>(std::move(source));
  return Open(std::move(wrapper), props, std::move(metadata));
}
#endif

std::unique_ptr<ParquetFileReader> ParquetFileReader::OpenFile(
    const std::string& path, s3selectEngine::rgw_s3select_api* rgw, bool memory_map, const ReaderProperties& props,
    std::shared_ptr<FileMetaData> metadata) {
  std::shared_ptr<::arrow::io::RandomAccessFile> source;
  if (memory_map) {
    PARQUET_ASSIGN_OR_THROW(
        source, ::arrow::io::MemoryMappedFile::Open(path, ::arrow::io::FileMode::READ));//GAL change that also, or to remove?
  } else {
    PARQUET_ASSIGN_OR_THROW(source,
                            ::arrow::io::ceph::ReadableFile::Open(path, rgw, props.memory_pool()));
  }

  return Open(std::move(source), props, std::move(metadata));
}

void ParquetFileReader::Open(std::unique_ptr<ParquetFileReader::Contents> contents) {
  contents_ = std::move(contents);
}

void ParquetFileReader::Close() {
  if (contents_) {
    contents_->Close();
  }
}

std::shared_ptr<FileMetaData> ParquetFileReader::metadata() const {
  return contents_->metadata();
}

std::shared_ptr<RowGroupReader> ParquetFileReader::RowGroup(int i) {
  if (i >= metadata()->num_row_groups()) {
    std::stringstream ss;
    ss << "Trying to read row group " << i << " but file only has "
       << metadata()->num_row_groups() << " row groups";
    throw ParquetException(ss.str());
  }
  return contents_->GetRowGroup(i);
}

void ParquetFileReader::PreBuffer(const std::vector<int>& row_groups,
                                  const std::vector<int>& column_indices,
                                  const ::arrow::io::IOContext& ctx,
                                  const ::arrow::io::CacheOptions& options) {
  // Access private methods here
  SerializedFile* file =
      ::arrow::internal::checked_cast<SerializedFile*>(contents_.get());
  file->PreBuffer(row_groups, column_indices, ctx, options);
}

// ----------------------------------------------------------------------
// File metadata helpers

std::shared_ptr<FileMetaData> ReadMetaData(
    const std::shared_ptr<::arrow::io::RandomAccessFile>& source) {
  return ParquetFileReader::Open(source)->metadata();
}

// ----------------------------------------------------------------------
// File scanner for performance testing
#if GAL_ScanAllValues_is_no_declare
int64_t ScanFileContents(std::vector<int> columns, const int32_t column_batch_size,
                         ParquetFileReader* reader) {
  std::vector<int16_t> rep_levels(column_batch_size);
  std::vector<int16_t> def_levels(column_batch_size);

  int num_columns = static_cast<int>(columns.size());

  // columns are not specified explicitly. Add all columns
  if (columns.size() == 0) {
    num_columns = reader->metadata()->num_columns();
    columns.resize(num_columns);
    for (int i = 0; i < num_columns; i++) {
      columns[i] = i;
    }
  }

  std::vector<int64_t> total_rows(num_columns, 0);

  for (int r = 0; r < reader->metadata()->num_row_groups(); ++r) {
    auto group_reader = reader->RowGroup(r);
    int col = 0;
    for (auto i : columns) {
      std::shared_ptr<ColumnReader> col_reader = group_reader->Column(i);
      size_t value_byte_size = GetTypeByteSize(col_reader->descr()->physical_type());
      std::vector<uint8_t> values(column_batch_size * value_byte_size);

      int64_t values_read = 0;
      while (col_reader->HasNext()) {
        int64_t levels_read =
            ScanAllValues(column_batch_size, def_levels.data(), rep_levels.data(),
                          values.data(), &values_read, col_reader.get());
        if (col_reader->descr()->max_repetition_level() > 0) {
          for (int64_t i = 0; i < levels_read; i++) {
            if (rep_levels[i] == 0) {
              total_rows[col]++;
            }
          }
        } else {
          total_rows[col] += levels_read;
        }
      }
      col++;
    }
  }

  for (int i = 1; i < num_columns; ++i) {
    if (total_rows[0] != total_rows[i]) {
      throw ParquetException("Parquet error: Total rows among columns do not match");
    }
  }

  return total_rows[0];
}
#endif

} //namespace ceph
} //namespace parquet 

/******************************************/
/******************************************/
/******************************************/
class column_reader_wrap
{

private:

  int64_t m_rownum;
  parquet::Type::type m_type;
  std::shared_ptr<parquet::ceph::RowGroupReader> m_row_group_reader;
  int m_row_grouop_id;
  uint16_t m_col_id;
  parquet::ceph::ParquetFileReader* m_parquet_reader;
  std::shared_ptr<parquet::ColumnReader> m_ColumnReader;
  bool m_end_of_stream;
  bool m_read_last_value;
  

public:

  enum class parquet_type
  {
    NA_TYPE,
    STRING,
    INT32,
    INT64,
    FLOAT,
    DOUBLE,
    TIMESTAMP,
    PARQUET_NULL
  };

  struct parquet_value
  {
    int64_t num;
    char *str; //str is pointing to offset in string which is NOT null terminated.
    uint16_t str_len;
    double dbl;
    parquet_type type;

    parquet_value():type(parquet_type::NA_TYPE){}
  };

  typedef struct parquet_value parquet_value_t;

  enum class parquet_column_read_state {PARQUET_OUT_OF_RANGE,PARQUET_READ_OK};

  private:
  parquet_value_t m_last_value;

  public:
  column_reader_wrap(std::unique_ptr<parquet::ceph::ParquetFileReader> & parquet_reader,uint16_t col_id);

  parquet::Type::type get_type();

  bool HasNext();//TODO template 

  int64_t ReadBatch(int64_t batch_size, int16_t* def_levels, int16_t* rep_levels,
                            parquet_value_t* values, int64_t* values_read);

  int64_t Skip(int64_t rows_to_skip);

  parquet_column_read_state Read(uint64_t rownum,parquet_value_t & value);

};

class parquet_file_parser
{

public:

  typedef std::vector<std::pair<std::string, column_reader_wrap::parquet_type>> schema_t;
  typedef std::set<uint16_t> column_pos_t;
  typedef std::vector<column_reader_wrap::parquet_value_t> row_values_t;

  typedef column_reader_wrap::parquet_value_t parquet_value_t;
  typedef column_reader_wrap::parquet_type parquet_type;

private:

  std::string m_parquet_file_name;
  uint32_t m_num_of_columms;
  uint64_t m_num_of_rows;
  uint64_t m_rownum;
  schema_t m_schm;
  int m_num_row_groups;
  std::shared_ptr<parquet::FileMetaData> m_file_metadata;
  std::unique_ptr<parquet::ceph::ParquetFileReader> m_parquet_reader;
  std::vector<column_reader_wrap*> m_column_readers;
  s3selectEngine::rgw_s3select_api* m_rgw_s3select_api;

  public:

  parquet_file_parser(std::string parquet_file_name,s3selectEngine::rgw_s3select_api* rgw_api) : 
                                   m_parquet_file_name(parquet_file_name),
                                   m_num_of_columms(0),
                                   m_num_of_rows(0),
                                   m_rownum(0),
                                   m_num_row_groups(0),
                                   m_rgw_s3select_api(rgw_api)
                                   
                                   
  {
    load_meta_data();
  }

  ~parquet_file_parser()
  {
    for(auto r : m_column_readers)
    {
      delete r;
    }
  }

  int load_meta_data()
  {
    m_parquet_reader = parquet::ceph::ParquetFileReader::OpenFile(m_parquet_file_name,m_rgw_s3select_api,false);
    m_file_metadata = m_parquet_reader->metadata();
    m_num_of_columms = m_parquet_reader->metadata()->num_columns();
    m_num_row_groups = m_file_metadata->num_row_groups();
    m_num_of_rows = m_file_metadata->num_rows();

    for (uint32_t i = 0; i < m_num_of_columms; i++)
    {
      parquet::Type::type tp = m_file_metadata->schema()->Column(i)->physical_type();
      std::pair<std::string, column_reader_wrap::parquet_type> elm;

      switch (tp)
      {
      case parquet::Type::type::INT32:
        elm = std::pair<std::string, column_reader_wrap::parquet_type>(m_file_metadata->schema()->Column(i)->name(), column_reader_wrap::parquet_type::INT32);
        m_schm.push_back(elm);
        break;

      case parquet::Type::type::INT64:
        elm = std::pair<std::string, column_reader_wrap::parquet_type>(m_file_metadata->schema()->Column(i)->name(), column_reader_wrap::parquet_type::INT64);
        m_schm.push_back(elm);
        break;

      case parquet::Type::type::FLOAT:
        elm = std::pair<std::string, column_reader_wrap::parquet_type>(m_file_metadata->schema()->Column(i)->name(), column_reader_wrap::parquet_type::FLOAT);
        m_schm.push_back(elm);
        break;

      case parquet::Type::type::DOUBLE:
        elm = std::pair<std::string, column_reader_wrap::parquet_type>(m_file_metadata->schema()->Column(i)->name(), column_reader_wrap::parquet_type::DOUBLE);
        m_schm.push_back(elm);
        break;

      case parquet::Type::type::BYTE_ARRAY:
        elm = std::pair<std::string, column_reader_wrap::parquet_type>(m_file_metadata->schema()->Column(i)->name(), column_reader_wrap::parquet_type::STRING);
        m_schm.push_back(elm);
        break;

      default:
        {
        std::stringstream err;
        err << "some parquet type not supported";
        throw std::runtime_error(err.str());
        }
      }

      m_column_readers.push_back(new column_reader_wrap(m_parquet_reader,i));
    }

    return 0;
  }

  bool end_of_stream()
  {

    if (m_rownum > (m_num_of_rows-1))
      return true;
    return false;
  }

  uint64_t get_number_of_rows()
  {
    return m_num_of_rows;
  }

  uint64_t rownum()
  {
    return m_rownum;
  }

  bool increase_rownum()
  {
    if (end_of_stream())
      return false;

    m_rownum++;
    return true;
  }

  uint64_t get_rownum()
  {
    return m_rownum;
  }

  uint32_t get_num_of_columns()
  {
    return m_num_of_columms;
  }

  int get_column_values_by_positions(column_pos_t positions, row_values_t &row_values)
  {
    column_reader_wrap::parquet_value_t column_value;
    row_values.clear();

    for(auto col : positions)
    {
      if((col)>=m_num_of_columms)
      {//TODO should verified upon syntax phase 
        //TODO throw exception
        return -1;
      }
      auto status = m_column_readers[col]->Read(m_rownum,column_value);
      if(status == column_reader_wrap::parquet_column_read_state::PARQUET_OUT_OF_RANGE) return -1;
      row_values.push_back(column_value);//TODO intensive (should move)
    }
    return 0;
  }

  schema_t get_schema()
  {
    return m_schm;
  }
};

/******************************************/


  column_reader_wrap::column_reader_wrap(std::unique_ptr<parquet::ceph::ParquetFileReader> & parquet_reader,uint16_t col_id):
  m_rownum(-1),
  m_type(parquet::Type::type::UNDEFINED),
  m_row_grouop_id(0),
  m_col_id(col_id),
  m_end_of_stream(false),
  m_read_last_value(false)
  {
    m_parquet_reader = parquet_reader.get();
    m_row_group_reader = m_parquet_reader->RowGroup(m_row_grouop_id);
    m_ColumnReader = m_row_group_reader->Column(m_col_id);
  }

  parquet::Type::type column_reader_wrap::get_type()
  {//TODO if UNDEFINED 
    return m_parquet_reader->metadata()->schema()->Column(m_col_id)->physical_type();
  }

  bool column_reader_wrap::HasNext()//TODO template 
  {
    parquet::Int32Reader* int32_reader;
    parquet::Int64Reader* int64_reader;
    parquet::FloatReader* float_reader;
    parquet::DoubleReader* double_reader;
    parquet::ByteArrayReader* byte_array_reader;

    switch (get_type())
    {
    case parquet::Type::type::INT32:
      int32_reader = static_cast<parquet::Int32Reader *>(m_ColumnReader.get());
      return int32_reader->HasNext();
      break;

    case parquet::Type::type::INT64:
      int64_reader = static_cast<parquet::Int64Reader *>(m_ColumnReader.get());
      return int64_reader->HasNext();
      break;

    case parquet::Type::type::FLOAT:
      float_reader = static_cast<parquet::FloatReader *>(m_ColumnReader.get());
      return float_reader->HasNext();
      break;

    case parquet::Type::type::DOUBLE:
      double_reader = static_cast<parquet::DoubleReader *>(m_ColumnReader.get());
      return double_reader->HasNext();
      break;

    case parquet::Type::type::BYTE_ARRAY:
      byte_array_reader = static_cast<parquet::ByteArrayReader *>(m_ColumnReader.get());
      return byte_array_reader->HasNext();
      break;

    default:

        std::stringstream err;
        err << "HasNext():" << "wrong type or type not exist" << std::endl;
        throw std::runtime_error(err.str());

      return false;
      //TODO throw exception
    }

    return false;
  }

  int64_t column_reader_wrap::ReadBatch(int64_t batch_size, int16_t* def_levels, int16_t* rep_levels,
                            parquet_value_t* values, int64_t* values_read)
  {
    parquet::Int32Reader* int32_reader;
    parquet::Int64Reader* int64_reader;
    parquet::FloatReader* float_reader;
    parquet::DoubleReader* double_reader;
    parquet::ByteArrayReader* byte_array_reader;

    parquet::ByteArray str_value;
    int64_t rows_read;
    int32_t i32_val;

    auto error_msg = [&](std::exception &e)
    {
      std::stringstream err;
      err << "what() :" << e.what() << std::endl;
      err << "failed to parse column id:" << this->m_col_id << " name:" <<this->m_parquet_reader->metadata()->schema()->Column(m_col_id)->name();
      return err;
    };
	int16_t defintion_level;
	int16_t repeat_level;

    switch (get_type())
    {
    case parquet::Type::type::INT32:
      int32_reader = static_cast<parquet::Int32Reader *>(m_ColumnReader.get());
      try {
	rows_read = int32_reader->ReadBatch(1, &defintion_level, &repeat_level, &i32_val , values_read);
      	if(defintion_level == 0)
      	{
		values->type = column_reader_wrap::parquet_type::PARQUET_NULL;
      	} else
      	{
      		values->num = i32_val;
      		values->type = column_reader_wrap::parquet_type::INT32;
      	}
      }
      catch(std::exception &e)
      {
         throw std::runtime_error(error_msg(e).str());
      }

      break;

    case parquet::Type::type::INT64:
      int64_reader = static_cast<parquet::Int64Reader *>(m_ColumnReader.get());
      try{
        rows_read = int64_reader->ReadBatch(1, &defintion_level, &repeat_level, (int64_t *)&(values->num), values_read);
      	if(defintion_level == 0)
      	{
		values->type = column_reader_wrap::parquet_type::PARQUET_NULL;
      	} else
      	{
		auto logical_type = m_parquet_reader->metadata()->schema()->Column(m_col_id)->logical_type();

                if (logical_type.get()->type() == parquet::LogicalType::Type::type::TIMESTAMP) //TODO missing sub-type (milli,micro)
                        values->type = column_reader_wrap::parquet_type::TIMESTAMP;
                else
                        values->type = column_reader_wrap::parquet_type::INT64;
      	}
      }
      catch(std::exception &e)
      {
         throw std::runtime_error(error_msg(e).str());
      }
      break;

    case parquet::Type::type::FLOAT:
        float_reader = static_cast<parquet::FloatReader *>(m_ColumnReader.get());
      try{
	float data_source_float = 0;
      	rows_read = float_reader->ReadBatch(1, &defintion_level, &repeat_level, &data_source_float , values_read);//TODO proper cast
      	if(defintion_level == 0)
      	{
		values->type = column_reader_wrap::parquet_type::PARQUET_NULL;
      	} else
      	{
      		values->type = column_reader_wrap::parquet_type::DOUBLE;
		values->dbl = data_source_float;

      	}
      }
      catch(std::exception &e)
      {
         throw std::runtime_error(error_msg(e).str());
      }
      break;

    case parquet::Type::type::DOUBLE:
        double_reader = static_cast<parquet::DoubleReader *>(m_ColumnReader.get());
      try{
      	rows_read = double_reader->ReadBatch(1, &defintion_level, &repeat_level, (double *)&(values->dbl), values_read);
      	if(defintion_level == 0)
      	{
		values->type = column_reader_wrap::parquet_type::PARQUET_NULL;
      	} else
      	{
      		values->type = column_reader_wrap::parquet_type::DOUBLE;
      	}
      }
      catch(std::exception &e)
      {
         throw std::runtime_error(error_msg(e).str());
      }
      break;

    case parquet::Type::type::BYTE_ARRAY:
      byte_array_reader = static_cast<parquet::ByteArrayReader *>(m_ColumnReader.get());
      try{
        rows_read = byte_array_reader->ReadBatch(1, &defintion_level, &repeat_level, &str_value , values_read);
      	if(defintion_level == 0)
      	{	
		values->type = column_reader_wrap::parquet_type::PARQUET_NULL;
      	} else
      	{
		values->type = column_reader_wrap::parquet_type::STRING;
      		values->str = (char*)str_value.ptr;
      		values->str_len = str_value.len;
      	}
      }
      catch(std::exception &e)
      {
         throw std::runtime_error(error_msg(e).str());
      }
      break;

    default:
      {
        std::stringstream err;
        err << "wrong type" << std::endl;
        throw std::runtime_error(err.str());
      }

    }

    return rows_read;
  }

  int64_t column_reader_wrap::Skip(int64_t rows_to_skip)
  {
    parquet::Int32Reader* int32_reader;
    parquet::Int64Reader* int64_reader;
    parquet::DoubleReader* double_reader;
    parquet::FloatReader* float_reader;
    parquet::ByteArrayReader* byte_array_reader;

    parquet::ByteArray str_value;
    int64_t rows_read;

    auto error_msg = [&](std::exception &e)
    {
      std::stringstream err;
      err << "what() :" << e.what() << std::endl;
      err << "failed to parse column id:" << this->m_col_id << " name:" <<this->m_parquet_reader->metadata()->schema()->Column(m_col_id)->name();
      return err;
    };

    switch (get_type())
    {
    case parquet::Type::type::INT32:
      int32_reader = static_cast<parquet::Int32Reader *>(m_ColumnReader.get());
      try{
        rows_read = int32_reader->Skip(rows_to_skip);
      }
      catch(std::exception &e)
      {
        throw std::runtime_error(error_msg(e).str());
      }
      break;

    case parquet::Type::type::INT64:
      int64_reader = static_cast<parquet::Int64Reader *>(m_ColumnReader.get());
      try{
        rows_read = int64_reader->Skip(rows_to_skip);
      }
      catch(std::exception &e)
      {
        throw std::runtime_error(error_msg(e).str());
      }
      break;

    case parquet::Type::type::FLOAT:
      float_reader = static_cast<parquet::FloatReader *>(m_ColumnReader.get());
      try {
        rows_read = float_reader->Skip(rows_to_skip);
      }
      catch(std::exception &e)
      {
        throw std::runtime_error(error_msg(e).str());
      }
      break;

    case parquet::Type::type::DOUBLE:
      double_reader = static_cast<parquet::DoubleReader *>(m_ColumnReader.get());
      try {
        rows_read = double_reader->Skip(rows_to_skip);
      }
      catch(std::exception &e)
      {
        throw std::runtime_error(error_msg(e).str());
      }
      break;

    case parquet::Type::type::BYTE_ARRAY:
      byte_array_reader = static_cast<parquet::ByteArrayReader *>(m_ColumnReader.get());
      try{
      	rows_read = byte_array_reader->Skip(rows_to_skip);
      }
      catch(std::exception &e)
      {
        throw std::runtime_error(error_msg(e).str());
      }
      break;
    
    default:
      {
        std::stringstream err;
        err << "wrong type" << std::endl;
        throw std::runtime_error(err.str());
      }
    }

    return rows_read;
  }


  column_reader_wrap::parquet_column_read_state column_reader_wrap::Read(const uint64_t rownum,parquet_value_t & value)
  {
    int64_t values_read = 0;

    if (m_rownum < (int64_t)rownum)
    { //should skip
      m_read_last_value = false;

      //TODO what about Skip(0)
      uint64_t skipped_rows = Skip(rownum - m_rownum -1);
      m_rownum += skipped_rows;

      while (((m_rownum+1) < (int64_t)rownum) || HasNext() == false)
      {
        uint64_t skipped_rows = Skip(rownum - m_rownum -1);
        m_rownum += skipped_rows;

        if (HasNext() == false)
        {
          if ((m_row_grouop_id + 1) >= m_parquet_reader->metadata()->num_row_groups())
          {
            m_end_of_stream = true;
            return column_reader_wrap::parquet_column_read_state::PARQUET_OUT_OF_RANGE;//end-of-stream
          }
          else
          {
            m_row_grouop_id++;
            m_row_group_reader = m_parquet_reader->RowGroup(m_row_grouop_id);
            m_ColumnReader = m_row_group_reader->Column(m_col_id);
          }
        }
      } //end-while

      ReadBatch(1, nullptr, nullptr, &m_last_value, &values_read);
      m_read_last_value = true;
      m_rownum++;
      value = m_last_value;
    }
    else
    {
      if (m_read_last_value == false)
      {
        ReadBatch(1, nullptr, nullptr, &m_last_value, &values_read);
        m_read_last_value = true;
        m_rownum++;
      }

      value = m_last_value;
    }

    return column_reader_wrap::parquet_column_read_state::PARQUET_READ_OK;
  }

#endif

