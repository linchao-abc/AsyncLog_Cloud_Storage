#include "Util.hpp"
#include <fstream>
#include <unistd.h>
#include <memory>

/*
LogFlush：纯虚基类，定义日志写入接口 Flush(const char*, size_t)。

StdoutFlush：把日志输出到标准输出流 std::cout。

FileFlush：把日志追加写入单个文件，并根据 g_conf_data->flush_log 决定是否 fflush/fsync。

RollFileFlush：支持按“最大文件大小”自动滚动到新文件，文件名包含时间戳 + 序号；在写入时同样有 fflush/fsync 策略。

LogFlushFactory：根据传入的具体 FlushType（如 FileFlush、RollFileFlush）和构造参数动态创建对应对象，返回 std::shared_ptr<LogFlush>。
*/

extern mylog::Util::JsonData* g_conf_data;

namespace mylog{
    class LogFlush {
        public:
            using ptr = std::shared_ptr<LogFlush>;
            virtual ~LogFlush() {}
            virtual void Flush(const char *data, size_t len) = 0; // 不同的写方式Flush的实现不同
    };

    class StdoutFlush : public LogFlush {
        public:
            using ptr = std::shared_ptr<StdoutFlush>;
            void Flush(const char* data, size_t len) override{
                std::cout.write(data, len); // 你告诉它从 data 开始，写出 len 字节，不管中间有没有 \0
            }
    };

    class FileFlush : public LogFlush {
        public:
            using ptr = std::shared_ptr<FileFlush>;
            // 创建目录，打开文件
            FileFlush(const std::string &filename) : filename_(filename) {
                // 创建所给目录
                Util::File::CreateDirectory(Util::File::Path(filename));
                // 打开文件
                fs_ = fopen(filename.c_str(), "ab"); // a表示追加模式， b表示二进制模式
                if(fs_==NULL) {
                    std::cout<< __FILE__ << __LINE__ << "open log file failed" << std::endl;
                    perror(NULL);
                }
            }

            void Flush(const char* data, size_t len) override{
                fwrite(data, 1, len, fs_);
                if(ferror(fs_)) {
                    std::cout << __FILE__ <<__LINE__ << "write log file failed" <<std::endl;
                    perror(NULL);
                }
                if(g_conf_data->flush_log == 1) {
                    if(fflush(fs_) == EOF) {
                        std::cout << __FILE__ << __LINE__ << "ffulsh file failed" << std::endl;
                        perror(NULL);
                    }
                } else if(g_conf_data->flush_log == 2) {
                    fflush(fs_);
                    fsync(fileno(fs_));
                }
            }
        private:
            std::string filename_;
            FILE* fs_ = NULL;
    };

    class RollFileFlush : public LogFlush{
        public:
            using ptr = std::shared_ptr<RollFileFlush>;
            RollFileFlush(const std::string &filename, size_t max_size) 
                : max_size_(max_size), basename_(filename) {
                // 创建目录
                Util::File::CreateDirectory(Util::File::Path(filename));
            }

            void Flush(const char* data, size_t len) {
                // 确认文件大小不满足滚动需求
                InitLogFile();
                // 向文件写入内容
                fwrite(data, 1, len, fs_);
                if(ferror(fs_)) {
                    std::cout << __FILE__ << __LINE__ << "fflush file failed" << std::endl;
                    perror(NULL);
                }
                cur_size_ += len;
                if(g_conf_data->flush_log == 1) {
                    if(fflush(fs_)) {
                        std::cout << __FILE__ << __LINE__ << "fflush file failed" << std::endl;
                        perror(NULL);
                    }
                }else if(g_conf_data->flush_log == 2) {
                    fflush(fs_);
                    fsync(fileno(fs_));
                }
            }

        private:
            void InitLogFile() {
                if(fs_==NULL || cur_size_ >= max_size_) {
                    if(fs_ != NULL) {
                        fclose(fs_);
                        fs_ = NULL;
                    }
                    std::string filename = CreateFilename();
                    fs_ = fopen(filename.c_str(), "ab"); // 文件如果不存在会自动创建并打开
                    if(fs_ == NULL) {
                        std::cout << __FILE__ << __LINE__ << "open file failed" << std::endl;
                        perror(NULL);
                    }
                    cur_size_ = 0;
                }
            }

            std::string CreateFilename() {
                time_t time_ = Util::Date::Now();
                struct tm t;
                localtime_r(&time_, &t);
                std::string filename = basename_;
                filename += std::to_string(t.tm_year + 1900);
                filename += std::to_string(t.tm_mon + 1);
                filename += std::to_string(t.tm_mday);
                filename += std::to_string(t.tm_hour + 1);
                filename += std::to_string(t.tm_min + 1);
                filename += std::to_string(t.tm_sec + 1) + '-' +
                            std::to_string(cnt_++) + ".log";
                return filename;
            }

        private:
            size_t cnt_ = 1;
            size_t cur_size_ = 0;
            size_t max_size_;
            std::string basename_;
            FILE* fs_ = NULL;
    };

    class LogFlushFactory {
        public:
            using ptr = std::shared_ptr<LogFlushFactory>;
            template <typename FlushType, typename... Args>
            static std::shared_ptr<LogFlush> CreateLog(Args &&...args) {
                return std::make_shared<FlushType>(std::forward<Args>(args)...);
            }
    };
}
