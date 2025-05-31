#pragma once
#include <iostream>
#include <sys/stat.h>
#include <fstream>
#include <sys/types.h>
#include <json/json.h>
using std::cout;
using std::endl;

namespace mylog {
    namespace Util {
        class Date {
        public:
            // 设为静态成员函数，可以不创建实例直接调用，返回当前的时间戳
            static time_t Now() { return time(nullptr); }
        };

        class File {
        public:
            // 判断文件是否存在
            static bool Exists(const std::string &filename) {
                // st用于保存文件的元数据，如文件的大小，类型，权限，修改时间等
                struct stat st;
                // 将filename文件的信息存储于st，成功返回0，失败返回-1
                return ( 0 == stat(filename.c_str(), &st));   
            }
            // 给定一个完整路径，提取该路径中目录部分（包括最后的斜杠），如果没有目录路径，返回空字符串。
            static std::string Path(const std::string &filename) {
                if(filename.empty()) return "";
                // 在字符串 filename 中，查找最后一次出现字符集 "/\\" 中任意一个字符的位置（索引）。
                // 如果没找到则返回std::string::npos
                int pos = filename.find_last_of("/\\");
                if(pos != std::string::npos) return filename.substr(0, pos+1);
                return "";
            }

            // 递归创建目录的功能，即如果路径中的上级目录不存在，则逐级创建，直到最终完整路径对应的目录被创建。
            static void CreateDirectory(const std::string &pathname) {
                if(pathname.empty()) {
                    perror("文件所在路径为空：");
                }
                // 文件不存在则创建
                if(!Exists(pathname)) {
                    size_t pos, index = 0;
                    size_t size = pathname.size();
                    while(index < size) {
                        pos = pathname.find_first_of("/\\", index);
                        if(pos == std::string::npos) {
                            mkdir(pathname.c_str(), 0775);
                            return;
                        }
                        if(pos == index) {
                            index = pos + 1;
                            continue;
                        }

                        std::string sub_path = pathname.substr(0, pos);
                        if(sub_path == "." || sub_path == "..") {
                            index = pos + 1;
                            continue;
                        }
                        if(Exists(sub_path)) {
                            index = pos + 1;
                            continue;
                        }
                        mkdir(sub_path.c_str(), 0775);
                        index = pos + 1;
                    }
                }
            }

            // 返回文件的大小
            int64_t FileSize(std::string filename) {
                struct stat s;
                auto ret = stat(filename.c_str(), &s);
                if(ret == -1) {
                    perror("Get file size failed");
                    return -1;
                }
                return s.st_size;
            }

            // 获取文件内容，成功返回true，将内容写入content，失败则返回false
            bool GetContent(std::string *content, std::string filename) {
                // 以二进制模式打开文件
                std::ifstream ifs;
                ifs.open(filename.c_str(), std::ios::binary);
                if(ifs.is_open() == false) {
                    cout << "file open error" << endl;
                    return false;
                }
                // 读入content
                ifs.seekg(0, std::ios::beg); // 将输入文件流 ifs 的“读指针”移动到文件开头的位置
                size_t len = FileSize(filename);
                content->resize(len);
                ifs.read(&(*content)[0], len); // 将filename中的内容读取到content中
                // 检查输入文件流ifs状态是否良好
                if(!ifs.good()) {
                    cout << __FILE__ << __LINE__ << "-"
                        << "read file content error" << endl;
                    ifs.close();
                    return false;
                }
                ifs.close();
                return true;
            }
        };

        class JsonUtil {
        public:
            static bool Serialize(const Json::Value &val, std::string *str) {
                // 把传入的 Json::Value 对象转换成标准 JSON 格式字符串，并写入str指向的string
                // 建造者生成->建造者实例化json写对象->调用写对象中的接口进行序列化写入str
                Json::StreamWriterBuilder swb;
                std::unique_ptr<Json::StreamWriter> usw(swb.newStreamWriter());
                std::stringstream ss;
                if(usw->write(val, &ss) != 0) {
                    cout << "serialize error" << endl;
                    return false;
                }
                *str = ss.str();
                return true;
            }

            static bool UnSerialize(const std::string &str, Json::Value *val) {
                // 将 JSON 格式的字符串 反序列化（解析）成 Json::Value 对象
                // 操作方法与序列化相似
                Json::CharReaderBuilder crb;
                std::unique_ptr<Json::CharReader> ucr(crb.newCharReader());
                std::string err; // 用于保存解析失败时的错误描述
                if(ucr->parse(str.c_str(), str.c_str() + str.size(), val, &err) == false) {
                    cout << __FILE__ << __LINE__ << "parse error" << err << endl;
                    return false;
                }
                return true;
            }
        };

        struct JsonData{
            static JsonData* GetJsonData() {
                static JsonData* json_data = new JsonData;
                return json_data;
            }

        private:
            JsonData() {
                std::string content;
                mylog::Util::File file;
                if(file.GetContent(&content, "../../log_system/logs_code/config.conf") == false) {
                    cout << __FILE__ << __LINE__ << "open config.conf failed" << endl;
                    perror(NULL);
                }
                Json::Value root;
                mylog::Util::JsonUtil::UnSerialize(content, &root); // 反序列化，把内容转成json value格式
                buffer_size = root["buffer_size"].asInt64();
                threshold = root["threshold"].asInt64();
                linear_growth = root["linear_growth"].asInt64();
                flush_log = root["flush_log"].asInt64();
                backup_addr = root["backup_addr"].asString();
                backup_port = root["backup_port"].asInt();
                thread_count = root["thread_count"].asInt();
            }
        public:
            size_t buffer_size; // 缓冲区基础容量
            size_t threshold; // 倍数扩容阈值
            size_t linear_growth; // 线性增长容量
            size_t flush_log; // 控制日志同步到磁盘的时机，默认是0，1调用fflush，2调用fsync
            std::string backup_addr;
            uint16_t backup_port;
            size_t thread_count;
        };
    }
}