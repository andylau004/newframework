
// version 03: pipeline impl to sort large files.
// overlap IO with computing using threads.

#include <boost/noncopyable.hpp>
#include <boost/ptr_container/ptr_vector.hpp>
#include <boost/bind.hpp>
#include <boost/enable_shared_from_this.hpp>


#include "muduo/base/Timestamp.h"
#include "muduo/base/ThreadPool.h"
#include "muduo/base/BlockingQueue.h"


#include <algorithm>
#include <string>
//#include <ext/vstring.h>
#include <vector>

#include <assert.h>
#include <stdio.h>

#include <sys/resource.h>

#include <vector>
#include <thread>
#include <mutex>
#include <atomic>


#ifdef H_HAVE_BOOST
#include <boost/lockfree/queue.hpp>
#elif defined(H_HAVE_CAMERON314_CONCURRENTQUEUE)
#endif



#include "muduo/base/common.h"


#include "concurrentqueue.h"



extern void tst_mt_rdwr_task_queues();




typedef std::string string;
// typedef __gnu_cxx::__sso_string string;

using muduo::Timestamp;

class InputFile : boost::noncopyable
{
public:
    InputFile(const char* filename)
        : file_(fopen(filename, "rb"))
    {
        assert(file_);
        setbuffer(file_, buffer_, sizeof buffer_);
        fileName_ = filename;
    }
    const char* getfilename() {
        return fileName_.c_str();
    }

    ~InputFile()
    {
        fclose(file_);
    }

    bool readLine(string* line)
    {
        char buf[256];
        if (fgets_unlocked(buf, sizeof buf, file_))
        {
            line->assign(buf);
            return true;
        }
        else
        {
            return false;
        }
    }

    int read(char* buf, int size)
    {
        return fread_unlocked(buf, 1, size, file_);
    }

    std::string fileName_;

private:
    FILE* file_;
    char buffer_[64*1024];
};

const int kRecordSize = 100;
const int kKeySize = 10;

class OutputFile : boost::noncopyable
{
public:
    OutputFile(const char* filename)
        : file_(fopen(filename, "wb"))
    {
        assert(file_);
        setbuffer(file_, buffer_, sizeof buffer_);
    }

    ~OutputFile()
    {
        fclose(file_);
    }

    void writeLine(const string& line)
    {
        if (line.empty())
        {
            fwrite_unlocked("\n", 1, 1, file_);
        }
        else if (line[line.size() - 1] == '\n')
        {
            fwrite_unlocked(line.c_str(), 1, line.size(), file_);
        }
        else
        {
            fwrite_unlocked(line.c_str(), 1, line.size(), file_);
            fwrite_unlocked("\n", 1, 1, file_);
        }
    }

    void writeRecord(char (&record)[kRecordSize])
    {
        fwrite_unlocked(record, 1, kRecordSize, file_);
    }


private:
    FILE* file_;
    char buffer_[64*1024];
};

//const bool kUseReadLine = false;
const int kBatchRecords = 10000000;
const bool kUseReadLine = true;

void readInput(const char* filename, std::vector<string>* data)
{
    InputFile in(filename);
    string line;
    int64_t totalSize = 0;
    data->reserve(kBatchRecords);

    if (kUseReadLine)
    {
        ifstream fin(filename);
        string line;
        while( getline(fin, line) )
        {
            stringTrimEx(line, '\r');stringTrimEx(line, '\n');
            if (line.size()) {
                data->push_back(line);
                totalSize += line.size();

//                printf( "line=%s\n", line.c_str());
            }
        }

//        while (in.readLine(&line))
//        {
//            totalSize += line.size();
//            data->push_back(line);
//        }
    }
    else
    {
        char buf[kRecordSize];
        while (int n = in.read(buf, sizeof buf))
        {
            totalSize += n;
            line.assign(buf, n);
            data->push_back(line);
        }
    }
}

struct Key
{
    char key[kKeySize];
    int index;

    Key(const string& record, int idx)
        : index(idx)
    {
        memset(key, 0, array_size(key));
        memcpy(key, record.data(), record.size()/*sizeof key*/);
    }

    bool operator<(const Key& rhs) const
    {
        return ((convert<int>(key) - convert<int>(rhs.key)) < 0);
//        return memcmp(convert<int>(key), convert<int>(rhs.key), sizeof key) < 0;
    }
};

void sortWithKeys(const std::vector<string>& data, std::vector<Key>* keys)
{
    Timestamp start = Timestamp::now();
    keys->clear();
    keys->reserve(data.size());

    for (size_t i = 0; i < data.size(); ++i)
    {
        keys->push_back(Key(data[i], i));
    }
//    printf("keys size=%d\n", keys->size());
    // printf("make keys %f\n", data.size(), timeDifference(Timestamp::now(), start));

    std::sort(keys->begin(), keys->end());
}

typedef std::vector<string> Data;

class Task;
typedef boost::shared_ptr<Task> TaskPtr;

class Task : public boost::enable_shared_from_this<Task>
{
public:
    Task(muduo::BlockingQueue<TaskPtr>* queue)
        : queue_(queue),
          id_(s_created++),
          sorted_(false)
    {
        assert(muduo::CurrentThread::isMainThread());
        printf("Task %d\n", id_);
    }

    ~Task()
    {
        printf("~Task %d\n", id_);
    }

    bool read(InputFile& in)
    {
        assert(muduo::CurrentThread::isMainThread());
        sorted_ = false;

        Timestamp startRead = Timestamp::now();
        printf("task %d start read  %s\n", id_, startRead.toString().c_str());

        readInput(in.getfilename()/*in*/, &data_);
        printf( "data_.size()=%d\n", data_.size() );

        Timestamp readDone = Timestamp::now();
        printf("task %d read done   %s  %f  %zd \n",
               id_, readDone.toString().c_str(), timeDifference(readDone, startRead), data_.size());
        return !data_.empty();
    }

    void sort()
    {
        // assert(!muduo::CurrentThread::isMainThread());
        assert(!sorted_);
        Timestamp startSort = Timestamp::now();
        printf("task %d start sort  %s\n", id_, startSort.toString().c_str());

        sortWithKeys(data_, &keys_);

        int idx = 0;
        for (auto it = keys_.begin(); it != keys_.end(); idx ++, ++ it) {
            printf( "idx[%d]=%s\n", idx+1, it->key );
        }

        sorted_ = true;
        Timestamp sortDone = Timestamp::now();
        printf("task %d sort done   %s  %f\n",
               id_, sortDone.toString().c_str(), timeDifference(sortDone, startSort));
        queue_->put(shared_from_this());
    }

    void write(int batch)
    {
        assert(muduo::CurrentThread::isMainThread());
        assert(sorted_);

        Timestamp startWrite = Timestamp::now();
        printf("task %d start write %s\n", id_, startWrite.toString().c_str());
        {
            char output[256];
            snprintf(output, sizeof output, "tmp%d", batch);

            OutputFile saveToFile(output);
            for (/*std::vector<Key>::iterator*/auto it = keys_.begin(); it != keys_.end(); ++it)
            {
                saveToFile.writeLine(data_[it->index]);
            }
        }
        Timestamp writeDone = Timestamp::now();
        printf("task %d write done  %s  %f\n",
               id_, writeDone.toString().c_str(), timeDifference(writeDone, startWrite));
    }

    const Data& data() const
    {
        return data_;
    }

private:
    Data data_;
    std::vector<Key> keys_;
    muduo::BlockingQueue<TaskPtr>* queue_;
    int id_;
    bool sorted_;

    static int s_created;
};

int Task::s_created = 0;


int sortSplit(const char* filename)
{
    InputFile in(filename);
    muduo::BlockingQueue<TaskPtr> queue;
    muduo::ThreadPool threadPool;
    threadPool.start(2);
    int active = 0;

    // initialize
    {
        TaskPtr task(new Task(&queue));
        if (task->read(in))
        {
            threadPool.run(boost::bind(&Task::sort, task));
            active++;
        }
        else
        {   printf( " read failed!!!\n " );
            return 0;
        }

//        TaskPtr task2(new Task(&queue));
//        if (task2->read(in))
//        {
//            threadPool.run(boost::bind(&Task::sort, task2));
//            active++;
//        }
    }

    int batch = 0;
    while (active > 0)
    {
        TaskPtr task = queue.take();
        active--;

        task->write(batch++);

        if (task->read(in))
        {
            threadPool.run(boost::bind(&Task::sort, task));
            active++;
        }
    }
    return batch;
}

struct Record
{
    char data[kRecordSize];
    InputFile* input;

    Record(InputFile* in)
        : input(in)
    {
    }

    bool next()
    {
        return input->read(data, sizeof data) == kRecordSize;
    }

    bool operator<(const Record& rhs) const
    {
        // make_heap to build min-heap, for merging
        return memcmp(data, rhs.data, kKeySize) > 0;
    }
};

void merge(const int batch)
{
    printf("merge %d files\n", batch);

    boost::ptr_vector<InputFile> inputs;
    std::vector<Record> keys;

    for (int i = 0; i < batch; ++i)
    {
        char filename[128];
        snprintf(filename, sizeof filename, "tmp%d", i);
        inputs.push_back(new InputFile(filename));
        Record rec(&inputs.back());
        if (rec.next())
        {
            keys.push_back(rec);
        }
    }

    OutputFile out("output");
    std::make_heap(keys.begin(), keys.end());
    while (!keys.empty())
    {
        std::pop_heap(keys.begin(), keys.end());
        out.writeRecord(keys.back().data);

        if (keys.back().next())
        {
            std::push_heap(keys.begin(), keys.end());
        }
        else
        {
            keys.pop_back();
        }
    }
}

int tst_tp_1()
{
    tst_mt_rdwr_task_queues();
    return 1;

    bool kKeepIntermediateFiles = false;
    {
        // set max virtual memory to 4GB.
        size_t kOneGB = 1024*1024*1024;
        rlimit rl = { 4.0*kOneGB, 4.0*kOneGB };
        setrlimit(RLIMIT_AS, &rl);
    }

    Timestamp start = Timestamp::now();
    printf("sortSplit start %s\n", start.toString().c_str());

    // sort
    int batch = sortSplit("tstdata");

    Timestamp sortDone = Timestamp::now();
    printf("sortSplit done %f\n", timeDifference(sortDone, start));

    if (batch == 0) {
    } else if (batch == 1) {
        rename("tmp0", "output");
    } else {
        // merge
        merge(batch);
        Timestamp mergeDone = Timestamp::now();
        printf("mergeSplit %f\n", timeDifference(mergeDone, sortDone));
    }

    if (!kKeepIntermediateFiles)
    {
        for (int i = 0; i < batch; ++i)
        {
            char tmp[256];
            snprintf(tmp, sizeof tmp, "tmp%d", i);
            unlink(tmp);
        }
    }

    printf("total %f\n", timeDifference(Timestamp::now(), start));
}



class CBigString {
public:
    CBigString() {
    }
    CBigString( const char* lpszVal ) {
        m_str_val = lpszVal;
    }
    virtual ~CBigString() {
    }
public:

private:
    std::string   m_str_val;

};



// 安全无锁队列基准测试


#ifdef H_USE_BLOCKINGQUEUE
        muduo::BlockingQueue < CBigString >*  g_pending_task_ = nullptr;

#elif defined(H_HAVE_BOOST)
        boost::lockfree::queue<int64_t>*      g_pending_task_ = nullptr;

#elif defined(H_HAVE_CAMERON314_CONCURRENTQUEUE)
        moodycamel::ConcurrentQueue<int64_t>* g_pending_task_ = nullptr;

#else
        std::mutex            g_mtx_pendingtask;
        std::vector<int64_t>* g_pending_task_ = nullptr;

#endif


//const int64_t max_task_count = 10 * 1000;
const int64_t max_task_count = 100 * 10000;


class CAddThreadPool {
public:
    CAddThreadPool(int numThreads)
        : adder_latch_(numThreads),
//          handler_latch(numThreads),
          cur_work_id(0)//,
//          task_handler_thread(boost::bind(&CAddThreadPool::handler_threadFunc, this), std::string("handlertask  thread"))
    {
#ifdef H_USE_BLOCKINGQUEUE
        g_pending_task_ = new muduo::BlockingQueue< CBigString >();
#elif defined(H_HAVE_BOOST)
        const size_t kPendingFunctorCount = 1024 * 16;
        g_pending_task_ = new boost::lockfree::queue<int64_t>(kPendingFunctorCount);
#elif defined(H_HAVE_CAMERON314_CONCURRENTQUEUE)
        g_pending_task_ = new moodycamel::ConcurrentQueue<int64_t>();
#else
        g_pending_task_ = new std::vector<int64_t>();
#endif
        add_threads_.reserve(numThreads);
        handler_threads_.reserve(2);

        for ( int64_t i = 0; i < max_task_count; ++i ) {
            map_Task[i] = 0;
        }
    }
    virtual ~CAddThreadPool(){
    }
    void reset() {
        cur_work_id = 0;
    }
public:
    void start() {
        // add task thread start
        for (int i = 0; i < add_threads_.capacity(); ++i) {
            char name[128] = {0};
            sprintf(name, "addtask thread %d", i);

            add_threads_.push_back(new muduo::Thread(
                   boost::bind(&CAddThreadPool::add_threadFunc, this), std::string(name)));
        }
        for_each(add_threads_.begin(), add_threads_.end(), boost::bind(&muduo::Thread::start, _1));
        adder_latch_.wait();

        LOG_INFO << "adder_latch_.wait done";
        //

        ::usleep(200);

        // handle task thread start
        for (int i = 0; i < handler_threads_.capacity(); ++i) {
            char name[128] = {0};
            sprintf(name, "handletask thread %d", i);

            handler_threads_.push_back(new muduo::Thread(
                   boost::bind(&CAddThreadPool::handler_threadFunc, this), std::string(name)));
        }
        for_each(handler_threads_.begin(), handler_threads_.end(), boost::bind(&muduo::Thread::start, _1));
        //

//        task_handler_thread.start();
    }

public:
    void joinAll()
    {
        // 等待写 任务线程工作完成
        for_each(add_threads_.begin(), add_threads_.end(), boost::bind(&muduo::Thread::join, _1));
        LOG_INFO << "add_task threads work done";

        // 等待处理 任务线程工作完成
        for_each(handler_threads_.begin(), handler_threads_.end(), boost::bind(&muduo::Thread::join, _1));
//        task_handler_thread.join();
        LOG_INFO << "handler_task threads work done";
    }

private:
    // 添加任务线程
    void add_threadFunc() {
        LOG_DEBUG << " start work";
        adder_latch_.countDown();

        bool running = true;
        while (running)
        {
//            int64_t itmp = std::atomic_load(&cur_work_id);
            int64_t itmp = cur_work_id.load();
            if (itmp < max_task_count) {
                cur_work_id ++;

#ifdef H_USE_BLOCKINGQUEUE
        std::string tmp_data = ("12asdfasjdfkjlasdjfjoq2ui3ie3uqwrqwerqwerqwerqwerqwerqwer");
        tmp_data += tmp_data;tmp_data += tmp_data;tmp_data += tmp_data;tmp_data += tmp_data;

        CBigString tmp_big_string(tmp_data.c_str());
        g_pending_task_->put(tmp_big_string);

#elif defined(H_HAVE_BOOST)
        while (!g_pending_task_->push(itmp)) {// 75
        }
#elif defined(H_HAVE_CAMERON314_CONCURRENTQUEUE)// 63
        while (!g_pending_task_->enqueue(itmp)) {
        }
#else
        std::lock_guard<std::mutex> lock(g_mtx_pendingtask);
        g_pending_task_->push_back/*emplace_back*/(itmp);
#endif
//                LOG_DEBUG << "enqueue itmp=" << itmp;
            } else {
                LOG_INFO << "found task queue full, itmp=" << itmp << ", max_count=" << max_task_count
                         << ", cur_work_id=" << cur_work_id.load();
//                handler_latch.countDown();
                return;
            }
        }
    }

    // 处理任务线程
    void handler_threadFunc() {
//        handler_latch.wait();
        LOG_DEBUG << " begwork!!!";

        int64_t outval    = -1;
//        int     workcount = 0;

#ifdef H_USE_BLOCKINGQUEUE
        while (1) {
            if ( cur_work_id <= 0 || cur_workcount.load() >= max_task_count) {
//            if ( cur_work_id <= 0 /*workcount.load() >= max_task_count*/) {
                LOG_DEBUG << "handler_threadFunc out!!!!!!!!!!!!!";
                break;
            }
            cur_work_id --;
            cur_workcount ++;

            CBigString tmpRet;
            tmpRet = g_pending_task_->take();
        }

#elif defined(H_HAVE_BOOST)

        while (1)
        {
            if ( cur_work_id <= 0 /*cur_workcount >= max_task_count*/) {
                LOG_DEBUG << "handler_threadFunc out!!!!!!!!!!!!!";
                break;
            }
            while (g_pending_task_->pop(outval)) {
                auto itfind = map_Task.find(outval);
                if ( itfind != map_Task.end() ) {
                    cur_work_id --;
                    cur_workcount ++;
//                    LOG_DEBUG << "outval" << cur_workcount.load() << "=" << outval;
                } else {
                    LOG_ERROR << "dequeue outval=" << outval << " not found, some badthing!!!!";
                }
            }
        }

#elif defined(H_HAVE_CAMERON314_CONCURRENTQUEUE)

        while (1)
        {
            if ( cur_work_id <= 0 || cur_workcount.load() >= max_task_count) {
//            if ( cur_work_id <= 0 /*workcount.load() >= max_task_count*/) {
                LOG_DEBUG << "handler_threadFunc out!!!!!!!!!!!!!";
                break;
            }
            while ( g_pending_task_->try_dequeue(outval) ) {
                auto itfind = map_Task.find(outval);
                if ( itfind != map_Task.end() ) {
                    cur_work_id --;
                    cur_workcount ++;
                    LOG_DEBUG << "outval" << cur_workcount << "=" << outval;
                } else {
                    LOG_ERROR << "dequeue outval=" << outval << " not found, some badthing!!!!";
                }
            }
        }

#else
        {
            while (1)
            {
                if ( cur_work_id <= 0 || cur_workcount.load() >= max_task_count) {
                    LOG_DEBUG << "handler_threadFunc out!!!!!!!!!!!!!";
                    break;
                }

                std::vector<int64_t> functors;
                {
                    std::lock_guard<std::mutex> lock(g_mtx_pendingtask);
                    g_pending_task_->swap(functors);
                }

                for (size_t i = 0; i < functors.size(); ++i) {

                    auto outval = functors.at(i);
                    auto itfind = map_Task.find( outval );

                    if ( itfind != map_Task.end() ) {
                        cur_work_id --;
                        cur_workcount ++;
                        LOG_DEBUG << "outval" << cur_workcount << "=" << outval;
                    } else {
                        LOG_ERROR << "dequeue outval=" << outval << " not found, some badthing!!!!";
                    }
                }
            }
        }
#endif
        LOG_DEBUG << " endwork!!! cur_workcount=" << cur_workcount << ", cur_work_id=" << cur_work_id.load();
    }

private:


    // 业务线程已经处理的任务总个数
    std::atomic<std::int64_t> handle_data_count;

    std::map < int64_t, int > map_Task;

    std::atomic<std::int64_t> cur_work_id;
    std::atomic<std::int64_t> cur_workcount;

    muduo::CountDownLatch adder_latch_;
//    muduo::CountDownLatch handler_latch;

    // add data thread
    boost::ptr_vector<muduo::Thread> add_threads_;
    // handle data thread
    boost::ptr_vector<muduo::Thread> handler_threads_;

    // get data handler thread
//    muduo::Thread task_handler_thread;

//    std::vector< std::unique_ptr< muduo::Thread > > threads_;
};


// 多线程读写　测试任务队列　例子
void tst_mt_rdwr_task_queues() {

    Timestamp startRead = Timestamp::now();

    int workwheel = 10;
    workwheel = 1;
    for (auto idx = 0; idx < workwheel; ++idx) {

        CAddThreadPool add_threadpool(4);
        add_threadpool.start();
        add_threadpool.joinAll();

#ifdef H_USE_BLOCKINGQUEUE
//        tmpRet = g_pending_task_->take();

#elif defined(H_HAVE_BOOST)
        g_pending_task_->reserve(0);
#elif defined(H_HAVE_CAMERON314_CONCURRENTQUEUE)
        moodycamel::ConcurrentQueue<int64_t> ctmp;
        g_pending_task_->swap(ctmp);
#else
        g_pending_task_->clear();
#endif

        add_threadpool.reset();
    }
    Timestamp readDone = Timestamp::now();
//    printf("task %d read done   %s  %f  %zd \n",
//           id_, readDone.toString().c_str(), timeDifference(readDone, startRead), data_.size());

    double intervaltime = timeDifference(readDone, startRead);
    intervaltime *= 1000;

    LOG_DEBUG << "all cost time: " << intervaltime << ", cost: " << intervaltime / workwheel  << " task/ms";

//    for (int i = 0; i != 123; ++i)
//        g_q_ints.enqueue(i);

//    int item;
//    for (int i = 0; i != 123; ++i) {
//        q.try_dequeue(item);
//        assert(item == i);
//    }

}
