#include "bulk.h"

using namespace std;
using namespace bulk;

void Commands::push_back(string str)
{
    if(!cmds.size())
    {
        timestamp = time(nullptr);
    }
    cmds.push_back(str);
    ++metrics.commands;
}

void Commands::push_back_block(string str)
{
    if(!cmds.size())
    {
        timestamp = time(nullptr);
    }
    cmds.push_back(str);
    ++metrics.commands;
}

void Commands::clear()
{
    cmds.clear();
    metrics.commands = 0;
    metrics.blocks = 0;
}

Observer::Observer()
{
    //cout << "ctor Observer" << endl;
    run_flag = true;
}

Observer::~Observer()
{
    //cout << "dtor Observer" << endl;
    run_flag = false;
}

bool Observer::queue_not_empty()
{
    unique_lock<mutex> lk(m);
    return !q.empty();
}

Dumper::Dumper()
{
    //cout << "ctor Dumper" << endl;
}

Dumper::~Dumper()
{
    //cout << "dtor Dumper" << endl;
}

void Dumper::subscribe(Observer *ob)
{
    subs.push_back(ob);
}

void Dumper::notify(Commands &cmds)
{
    for (auto s : subs)
    {
        s->dump(cmds);
    }
}

void Dumper::dump_commands(Commands &cmd)
{
    notify(cmd);
}

void Dumper::stop_dumping()
{
    for (auto s : subs)
    {
        s->stop();
    }
}

ConsoleDumper::ConsoleDumper(shared_ptr<Dumper> dmp)
{
    //cout << "ctor ConsoleDumper" << endl;
    dmp->subscribe(this);
}

ConsoleDumper::~ConsoleDumper()
{
    //cout << "dtor ConsoleDumper" << endl;
}

void ConsoleDumper::dump(Commands &cmd)
{
    //cout << "ConsoleDumper::dump" << endl;
    {
        lock_guard<mutex> lg(m);
        q.push(cmd);
    }
    cv.notify_one();
}

void ConsoleDumper::stop()
{
    run_flag = false;
    cv.notify_all();
}

void ConsoleDumper::dumper(Metrics &metrics)
{
    while (run_flag || queue_not_empty())
    {
        unique_lock<mutex> lk(m);
        cv.wait(lk, [this]{return (!run_flag || !q.empty());});
        if(!run_flag && q.empty())
        {
            return;
        }

        auto commands = q.front();
        q.pop();
        lk.unlock();

        metrics += commands.metrics;
        bool is_first = true;
        cout << "bulk: ";
        for(auto s : commands.cmds)
        {
            if(is_first)
            {
                is_first = false;
            }
            else
            {
                cout << ", ";
            }
            cout << s;
        }
        cout << endl;
    }
}

FileDumper::FileDumper(shared_ptr<Dumper> dmp)
{
    //cout << "ctor FileDumper" << endl;
    unique_file_counter = 0;
    dmp->subscribe(this);
}

FileDumper::~FileDumper()
{
    //cout << "dtor FileDumper" << endl;
}

string FileDumper::get_unique_number()
{
    return to_string(++unique_file_counter);
}

void FileDumper::dump(Commands &cmd)
{
    //cout << "FileDumper::dump" << endl;
    {
        lock_guard<mutex> lg(m);
        q.push(cmd);
    }
    cv.notify_one();
}

void FileDumper::stop()
{
    run_flag = false;
    cv.notify_all();
}

void FileDumper::dumper(Metrics &metrics)
{
    while (run_flag || queue_not_empty())
    {
        unique_lock<mutex> lk(m);
        cv.wait(lk, [this]{return (!run_flag || !q.empty());});
        if(!run_flag && q.empty())
        {
            return;
        }

        auto cmds = q.front();
        q.pop();
        lk.unlock();

        metrics += cmds.metrics;

        string filename = "bulk" + to_string(cmds.timestamp) + "_" + get_unique_number() + ".log";
        ofstream of(filename);

        bool is_first = true;
        of << "bulk: ";
        for(auto s : cmds.cmds)
        {
            if(is_first)
            {
                is_first = false;
            }
            else
            {
                of << ", ";
            }
            of << s;
        }
        of << endl;
        of.close();
    }
}

BulkSessionProcessor::BulkSessionProcessor()
{
    blockFound = false;
    nestedBlocksCount = 0;
    lines_count = 0;
}


BulkContext::BulkContext()
{
    //cout << "ctor BulkContext" << endl;

    dumper = make_shared<Dumper>();
    conDumper = make_shared<ConsoleDumper>(dumper);
    fileDumper = make_shared<FileDumper>(dumper);

    Metrics log_metr, file1_metr, file2_metr;
    cdt = thread(&ConsoleDumper::dumper, this->conDumper, std::ref(log_metr));
    fdt1 = thread(&FileDumper::dumper, this->fileDumper, std::ref(file1_metr));
    fdt2 = thread(&FileDumper::dumper, this->fileDumper, std::ref(file2_metr));
}

BulkContext::~BulkContext()
{
    cout << "dtor BulkContext" << endl;
    dumper->stop_dumping();
    cdt.join();
    fdt1.join();
    fdt2.join();
}

void BulkContext::set_bulk_size(size_t bulk_size_)
{
    bulk_size = bulk_size_;
}
void BulkContext::add_line(string &cmd, BulkSessionProcessor &session_cmds, BulkSessionProcessor &shared_cmds)
{
    ++session_cmds.lines_count;
    if((cmd != "{") && !session_cmds.blockFound)
    {
        shared_cmds.cmds.push_back(cmd);

        if(shared_cmds.cmds.metrics.commands == bulk_size)
        {
            dump_block(shared_cmds);
        }
    }
    else
    {
        if(!session_cmds.blockFound)
        {
            session_cmds.blockFound = true;
            return;
        }

        if(cmd == "{")
        {
            ++session_cmds.nestedBlocksCount;
        }
        else if(cmd == "}")
        {
            if (session_cmds.nestedBlocksCount > 0)
            {
                --session_cmds.nestedBlocksCount;
            }
            else
            {
                dump_block(session_cmds);
                session_cmds.blockFound = false;
            }
        }
        else
        {
            session_cmds.cmds.push_back_block(cmd);
        }
    }
}

void BulkContext::dump_block(BulkSessionProcessor &commands)
{
    ++commands.cmds.metrics.blocks;
    dumper->dump_commands(commands.cmds);
    commands.metrics += commands.cmds.metrics;
    commands.cmds.clear();
}

void BulkContext::print_metrics()
{
//    cout << "main: " << lines_count << " lines, "
//                     << metrics.commands << " commands, "
//                     << metrics.blocks << " blocks"
//                     << endl;

    Metrics::print_metrics(log_metr, "log");
    Metrics::print_metrics(file1_metr, "file1");
    Metrics::print_metrics(file2_metr, "file2");
}
