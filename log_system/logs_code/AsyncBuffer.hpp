#pragma once
// 日志缓冲区类设计
#include <vector>
#include <string>
#include "Util.hpp"
#include <cassert>

extern mylog::Util::JsonData* g_conf_data;

namespace mylog{
    class Buffer{
        public:
            Buffer() : write_pos_(0), read_pos_(0) {
                buffer_.resize(g_conf_data->buffer_size);
            }

            void Push(const char *data, size_t len) {
                ToBeEnough(len); // 确保容量足够，不够则扩容
                // 开始写入
                std::copy(data, data + len, &buffer_[write_pos_]);
                write_pos_ += len;
            }

            size_t WriteableSize()  {
                return buffer_.size() - write_pos_; // 返回写空间剩余容量
            }

            size_t ReadableSize(){
                return write_pos_ - read_pos_;
            }

            char *ReadBegin(int len) {
                assert(len <= ReadableSize()); // 检查len是否小于等于可读空间
                return &buffer_[read_pos_]; // 返回读值指针
            }
            
            bool IsEmpty() {
                return write_pos_ == read_pos_;
            }

            void Swap(Buffer &buf) {
                buffer_.swap(buf.buffer_);
                std::swap(read_pos_, buf.read_pos_);
                std::swap(write_pos_, buf.write_pos_);
            }

            void MoveWritePos(int len) {
                assert(len <= WriteableSize());
                write_pos_ += len;
            }

            void MoveReadPos(int len) {
                assert(len <= ReadableSize());
                read_pos_ += len;
            }

            void Reset() {
                // 重置缓冲区
                write_pos_ = 0;
                read_pos_ = 0;
            }


        protected:
            void ToBeEnough(size_t len) {
                int buffersize = buffer_.size();
                if(len >= WriteableSize()) {
                    if(buffer_.size() < g_conf_data->threshold) {
                        buffer_.resize(2*buffer_.size() + buffersize); //倍数增长为原始容量的三倍
                    }
                    else {
                        buffer_.resize(g_conf_data->linear_growth + buffersize); //容量线性增长
                    }
                }
            }    

        protected:
            std::vector<char> buffer_; // 缓冲区
            size_t write_pos_; // 生产者此时的位置
            size_t read_pos_; // 消费者此时的位置
    };
}

