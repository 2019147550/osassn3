#include <iostream>
#include <fstream>
#include <string>
#include <cmath>
#include <vector>
#include <unistd.h>
#include <filesystem>
#include <queue>
#include <algorithm>
#define INT_MAX 2147483647
#define PATH_MAX 100000
// 스케쥴링 알고리즘
#define FCFS "fcfs"
#define RR "rr"
// 페이지 교체 알고리즘
#define LRU "lru"
#define SAMPLED_LRU "sampled"
#define CLOCK "clock"
#define TIME_QUANTUM 10
// ALLOCATION, ACCESS, RELEASE, NON-MEMORY, SLEEP, IOWAIT, NO-OP

// function name
#define ALLOCATION  "ALLOCATION";
#define ACCESS   "ACCESS";
#define RELEASE  "RELEASE";
#define NON_MEMORY   "NON-MEMORY";
#define SLEEP  "SLEEP";
#define IOWAIT  "IOWAIT";
#define NO_OP   "NO-OP";
using namespace std;
bool debug = false;
class Process;
class FCFSQueue;
class RRQueue;
char tmp[256];
string cwd;
string scheduling_algorithm = FCFS;
string page_replacement_algorithm = LRU;
string input_directory; // input path
string input_dir;
string page = LRU;
int total_event_num, vm_size, pm_size, page_size;
vector<Process> total_process;
vector<Process> sleep_list;
vector<Process> iowait_list;
vector<Process> running_list;
vector<vector<int>> will_input_list;
vector<vector<Process>> to_be_pushed_process;
vector<int> input_exist_cycle_list;
vector<vector<int>> input_list_at_cycle;
vector<vector<Process>>process_list_at_cpu_cycle;
int max_cycle_process = 1;
int total_page_fault = 0;
int end_process_count = 0;
int total_waiting_count = 0;
bool scheduled;

// pagetable 관리하기위한 class
class PageTable {
public:
    vector<int> table;
    vector<int> valid_bit;
    vector< vector<int> > reference_byte;
    vector<int> reference_bit;
    vector<int> reference_num;
    PageTable() {};
    PageTable(int size) {
        table = vector<int>(size, -1);
        valid_bit = vector<int>(size, -1);
        if (page == SAMPLED_LRU) {
            reference_bit = vector<int>(size, 0);
            vector<vector<int> > tmpvec(size, vector<int>(8, 0));
            reference_byte = tmpvec;
            reference_num = vector<int>(size, 0);
        }
    }
};
// process 의 정보를 담고 있는 class
class Process {
public:
    int pid;
    vector<vector<int>> codes;
    int total_code_num;
    int code_iterator = 0;
    string process_name;
    int is_sleeping;
    int cycle_time;
    bool end = false;
    int priority;
    int sleeping_time_left;
    vector<int> page_table;
    vector<int> reference_bit;
    vector<vector<int>> reference_byte;

    Process() {};


    Process(int pid, string code_dir, string pname, int size, int priority, int cycle_time) {
        this->pid = pid;
        this->cycle_time = cycle_time;
        char* process_dir = (char*)code_dir.c_str();
        ifstream File(process_dir);
        this->process_name = pname;

        File >> total_code_num;
        if(debug){
            cout << this->process_name << " will be generated\n";
        }
        for (int i = 0;i < total_code_num; i++) {
            int opcode, param;
            File >> opcode >> param;
            cout << opcode << " " << param << endl;
            vector<int> line;
            line.push_back(opcode);
            line.push_back(param);
            codes.push_back(line);
        }
        vector<int> vec(size, -1);
        this->priority = priority;
        if(debug){
            cout << "process "<< this->pid << " " << this->process_name << " priority " << priority << " generated\n";

        }

    }
    // 현재 실행해야 할 코드라인을 가져온다
    vector<int> get_line() {
        vector<int> line;
        line.push_back(codes[code_iterator][0]); //opcode
        line.push_back(codes[code_iterator][1]); //argument
        code_iterator++;
        if (code_iterator >= codes.size()) { // 마지막 코드라인인지
            line.push_back(0);
        }
        else {
            line.push_back(1);
        }
        return line;


    }



};
// sampled lru 에 필요한 reference byte를 위한 class
class ByteArray{
    public:
    vector<int> ref_byte;
    vector<int> saved = vector<int>(8,0);
    ByteArray(){
        for(int i=0;i<8;i++){
            ref_byte.push_back(0);
        }
    }
    // sampled lru에서 byte값비교를 위한 값 계산 함수이다.
    int get_sum(){
        int count_bit = 0;
        for(int i=0;i<ref_byte.size();i++){
            count_bit += pow(ref_byte[i]*2, i+1);
        }
       
        return count_bit;

    }
    // 새로운 ref bit을 저장해둔다
    void push_bit(int bit){
        vector<int> tmp_v(0);
        for(int i=1;i<saved.size();i++){
            tmp_v.push_back(saved[i]);

        }

        saved = tmp_v;
        saved.push_back(bit);
    }
    // 저장해둔 ref bit을 업데이트 해준다. 8사이클 주기
    void update(){
        ref_byte = saved;
        vector<int> new_saved;
        for(int i=0;i<8;i++){
            new_saved.push_back(0);
        }

        saved = new_saved;

    }
    void print_byte(){
        for(int i=0;i<ref_byte.size();i++){
            cout << ref_byte[i] << " ";
        }
        cout << endl;
    }
    void print_saved(){
         for(int i=0;i<saved.size();i++){
            cout << saved[i] << " ";
        }
        cout << endl;
    }

};
// 페이지 교체 알고리즘, 버디 시스템, 물리 메모리 구현을 위한 클래스이다.
class VMSystem{
    public:
    vector<vector<int>> pid_table;
    vector<vector<int>> aid_table;
    vector<vector<int>> reference_bit;
    vector<vector<int>> valid_bit;
    vector<int> pm_table;
    vector<vector<int>> page_num_table; // 각 프로세스당 pid의 size기록
    vector<vector<ByteArray>> reference_byte;
    vector<int> pid_count;
    vector<vector<int>> pid_to_aid;
    vector<int> pm_segment_size;
    vector<int> used_stack; // for lru 
    int aid_count;
    int clock_idx_point = -1;
    VMSystem(int pmsize, int vmsize, int page_size, int num_of_total_process){
        pid_table = vector<vector<int>>(num_of_total_process,vector<int>((vmsize/page_size),-1));
        page_num_table = vector<vector<int>>(num_of_total_process,vector<int>(0));
        aid_table = vector<vector<int>>(num_of_total_process,vector<int>((vmsize/page_size),-1));;
        reference_bit = vector<vector<int>>(num_of_total_process,vector<int>((vmsize/page_size),-1));;
        valid_bit = vector<vector<int>>(num_of_total_process,vector<int>((vmsize/page_size),-1));
        pm_table = vector<int>((pmsize/page_size), -1);
        pm_segment_size = vector<int>(1,(pmsize/page_size));
        pid_count = vector<int> (num_of_total_process, 0);
        pid_to_aid = vector<vector<int>> (num_of_total_process, vector<int>(0));
        aid_count = 0;
        for(int i=0;i<num_of_total_process;i++){
            queue<int> t_q;
            for(int j=0;j<8;j++){   
                t_q.push(0);
            }
            vector<ByteArray> byte_list;
            reference_byte.push_back(byte_list);
        }

    }
    // 메모리 정보를 file에 출력하는 함수이다.
    void print_memory_data(FILE*out, int cycle_count, string function, Process running_process, int page_id, vector<Process> on_handling){
    
        // line 1
        if(function=="ALLOCATION" || function == "ACCESS" || function == "RELEASE"){
            fprintf(out,"[%d Cycle] Input: Pid[%d] Function[%s] Page ID[%d] Page Num[%d]\n", cycle_count, running_process.pid, (char*)function.c_str(), page_id, page_num_table[running_process.pid][page_id]);
        }
        //NON MEMORY, SLEEP, IOWAIT
        if(function == "NON-MEMORY" || function == "SLEEP" || function == "IOWAIT"){
            fprintf(out,"[%d Cycle] Input: Pid[%d] Function[%s]\n", cycle_count, running_process.pid, (char*)function.c_str());
        }
        if(function == "NO-OP"){
            fprintf(out,"[%d Cycle] Input: Function[NO-OP]\n", cycle_count);
        }
        // line 2
        fprintf(out,"%-30s", ">> Physical Memory: ");
        fprintf(out,"|");
        for(int i=0;i<pm_table.size();i++){
            if(pm_table[i]!=-1){
                fprintf(out,"%d", pm_table[i]);
            }
            else{
                fprintf(out,"-");
            }
            if((i+1)/4 * 4 == i+1){
                fprintf(out,"|");
            }
        }
        fprintf(out,"\n");
       

        // line 3
        vector<int> sorted_pid = get_handled_process_pid(on_handling);
       
        for(int i=0;i<sorted_pid.size();i++){
            fprintf(out,">> pid(%d) %-20s", sorted_pid[i], " Page Table(PID): ");
            fprintf(out,"|");

            for(int j=0;j<pid_table[0].size();j++){
                if(pid_table[sorted_pid[i]][j]!=-1){
                    fprintf(out,"%d",pid_table[sorted_pid[i]][j]);
                }
                else{
                    fprintf(out,"-");
                }
                if((j+1)/4 * 4 == j+1){
                    fprintf(out,"|");
                }

            }
            fprintf(out,"\n");
            fprintf(out,">> pid(%d) %-20s", sorted_pid[i], " Page Table(AID): ");
            fprintf(out,"|");

            for(int j=0;j<aid_table[0].size();j++){
                if(aid_table[sorted_pid[i]][j]!=-1){
                    fprintf(out,"%d",aid_table[sorted_pid[i]][j]);
                }
                else{
                    fprintf(out,"-");
                }
                if((j+1)/4 * 4 == j+1){
                    fprintf(out,"|");
                }

            }
            fprintf(out,"\n");


            fprintf(out,">> pid(%d) %-20s", sorted_pid[i], " Page Table(Valid): ");
            fprintf(out,"|");

            for(int j=0;j<valid_bit[0].size();j++){
                if(valid_bit[sorted_pid[i]][j]!=-1 ){
                    fprintf(out,"%d",valid_bit[sorted_pid[i]][j]);
                }
                else{
                    fprintf(out,"-");
                }
                if((j+1)/4 * 4 == j+1){
                    fprintf(out,"|");
                }

            }
            fprintf(out,"\n");

            fprintf(out,">> pid(%d) %-20s", sorted_pid[i], " Page Table(Ref): ");
            fprintf(out,"|");

            for(int j=0;j<reference_bit[0].size();j++){
                if(reference_bit[sorted_pid[i]][j]!=-1 && page != LRU && pid_table[sorted_pid[i]][j]!=-1){
                    fprintf(out,"%d",reference_bit[sorted_pid[i]][j]);
                }
                else{
                    fprintf(out,"-");
                }
                if((j+1)/4 * 4 == j+1){
                    fprintf(out,"|");
                }

            }
            fprintf(out,"\n");

        }
        fprintf(out,"\n");


    }
    // 현재 메모리에 올라간 프로세스의 id를 가져오는 함수이다.
    vector<int> get_handled_process_pid(vector<Process> on_handling){
        vector<int> pid_list;
        
        for(int i=0;i<on_handling.size();i++){
            if(!on_handling[i].end){
                pid_list.push_back(on_handling[i].pid);
            }
        }
        sort(pid_list.begin(), pid_list.end());

        return pid_list;
    }
    void print_segment_size(){
        cout << "--- segment size ---\n";
        for(int i=0;i<pm_segment_size.size();i++){
            cout << pm_segment_size[i] << " ";
        }
        cout <<endl;

    }
    // page를 release하는 함수이다.
    void release_page(int page_id, int process_id){
        int target_aid = pid_to_aid[process_id][page_id];
        for(int i=0; i<pid_table[process_id].size();i++){
            if(pid_table[process_id][i] == page_id){
                pid_table[process_id][i] = -1;
                aid_table[process_id][i] = -1;
                valid_bit[process_id][i] = -1;


            }
        }
        for(int i=0; i<pm_table.size();i++){
            if(pm_table[i] == target_aid){
                pm_table[i] = -1;
            }
        }
        merge_buddy();
    }
    // aid를 물리 메모리에 할당한다.
    // 버디시스템을 이용한다.
    bool allocate_aid(int pid, int process_id){
        cout << "current aid count : " << aid_count << endl;
        int target_page_size = page_num_table[process_id][pid];
        int frame_size;
        bool found_perfect_segment = false;
        bool new_aid;
        int segment_iteration_count = 0;
        int found_segment_idx;
        int need_frame_amount = pm_size/page_size;
        cout << "pid to aid : ";
        for(int i=0;i<pid_to_aid[process_id].size();i++){
            cout << pid_to_aid[process_id][i] << " ";
        }
        cout << endl;
        int target_aid;
        if(pid_to_aid[process_id][pid] >= 0){
            target_aid = pid_to_aid[process_id][pid];
            new_aid = false;
        }
        else{
            target_aid = aid_count;
            new_aid = true;
        }
        
        while(true){
            if(target_page_size<= need_frame_amount && need_frame_amount/2 < target_page_size){
                break;
            }
            need_frame_amount /= 2;
        }
        if(debug){
            cout << "필요한 segment 크기 : " << need_frame_amount << endl;
        print_segment_size();
            cout << "found perfect segment : " << found_perfect_segment << endl;

            
        }
        for(int i=0;i<pm_segment_size.size();i++){
            if(pm_segment_size[i]==need_frame_amount){
                if(pm_table[segment_iteration_count]==-1){
                    found_perfect_segment = true;
                    found_segment_idx = i;
                    break;
                }
            }
            segment_iteration_count += pm_segment_size[i];
        }
        // aid_table, aid_count, pid_to_aid, pm_table, reference_bit
        if(found_perfect_segment){
            for(int i=0; i<pm_segment_size[found_segment_idx];i++){
                pm_table[segment_iteration_count+i] = target_aid;
            }
            // aid table update
            for(int i=0;i<aid_table[process_id].size();i++){
                if(pid_table[process_id][i]==pid){
                    aid_table[process_id][i] = target_aid;
                    valid_bit[process_id][i] = 1;

                }
            }
            pid_to_aid[process_id][pid] = target_aid;
            if(new_aid){
                aid_count ++;
            }
            if(debug){
                cout << "buddy를 쪼갤 필요가 없습니다\n";
                print_segment_size();
                cout << "buddy start : " << segment_iteration_count << endl; 
            }
            used_stack.push_back(target_aid);

            return true;
        }
        else{
            // buddy 나눠야함
            if(debug){
                cout << "적절한 buddy 크기가 없습니다.\n";
            }
            int min_segment_idx;
            int min_segment_iteration_count = 0;
            int min_segment_size = pm_size/page_size + 1;
            int segment_iteration_count = 0;
            bool segment_found = false;
            for(int i=0;i<pm_segment_size.size();i++){
                if(pm_segment_size[i] < min_segment_size && pm_table[segment_iteration_count] == -1 && pm_segment_size[i]>=need_frame_amount){
                    min_segment_idx = i;
                    min_segment_size = pm_segment_size[i];
                    min_segment_iteration_count = segment_iteration_count;
                    segment_found = true;
                    break;
                }
                segment_iteration_count += pm_segment_size[i];

            }
            // 적절한 크기의 segment 없을 때
            if(!segment_found){
                return false;
            }
            if(debug){
                cout << "buddy를 쪼갭니다\n";
            }
            // 버디 쪼개서 다시 리스트에 넣는 과정
            vector<int> tmp_segment;
            for(int i=0;i<min_segment_idx;i++){
                tmp_segment.push_back(pm_segment_size[i]);
            }
            int need_frame_copy = need_frame_amount;
            tmp_segment.push_back(need_frame_amount);
            tmp_segment.push_back(need_frame_amount);
            need_frame_amount *= 2;
            while(min_segment_size!= need_frame_amount){
                tmp_segment.push_back(need_frame_amount);
                need_frame_amount *= 2;
                cout << "현재 buddy 크기 : " << need_frame_amount << endl;
            }
            for(int i=min_segment_idx+1;i<pm_segment_size.size();i++){
                tmp_segment.push_back(pm_segment_size[i]);
            }
            pm_segment_size = tmp_segment;
            for(int i=0;i<need_frame_copy;i++){
                cout << i+segment_iteration_count <<" : " << target_aid << endl;
                pm_table[i+segment_iteration_count] = target_aid;
            }
            for(int i=0;i<aid_table[process_id].size();i++){
                if(pid_table[process_id][i]==pid){
                    aid_table[process_id][i] = target_aid;
                    valid_bit[process_id][i] = 1;
                }
            }
            print_segment_size();
            
            


        }
        pid_to_aid[process_id][pid] = aid_count;
        if(new_aid){
            aid_count ++;
        }
        used_stack.push_back(target_aid);

        return true;


    }
    // 물리 메모리에서 프로세스의 흔적을 없앤다.
    void remove_process_from_pm(int process_id){
        for(int i=0;i<pid_to_aid[process_id].size();i++){
            for(int j=0;j<pm_table.size();j++){
                if(pm_table[j] == pid_to_aid[process_id][i]){
                    pm_table[j] = -1;
                }
            }
        }
        merge_buddy();
    }
    // pm 에서 해당 aid -> -1로 만들어주고 해당 aid를 가진 page도 vaild bit을 -1으로 만들어준다
    void release_aid(int aid){
        for(int i=0;i<pm_table.size();i++){
            if(pm_table[i]==aid){
                pm_table[i] = -1;
            }
        }
        for(int i=0;i<aid_table.size();i++){
            for(int j=0;j<aid_table[0].size();j++){
                if(aid_table[i][j]== aid){
                    valid_bit[i][j] = 0;
                }
            }
        }
    }
    // pm에 존재하는 aid리스트 얻는 함수
    vector<int> get_on_pm_aid_list(){
        vector<int> on_pm_aid_list;
        for(int i=0;i<pm_table.size();i++){
           if( count(on_pm_aid_list.begin(), on_pm_aid_list.end(), pm_table[i]) <= 0 && pm_table[i] != -1){
               on_pm_aid_list.push_back(pm_table[i]);
           }
        }
        return on_pm_aid_list;
    }
    vector<int> uid_to_processid_pageid(int uid){
        vector<int> return_v;
        for(int i=0;i<aid_table.size();i++){
            for(int j=0;j<aid_table[i].size();j++){
                if(aid_table[i][j] == uid){
                    vector<int> tmp_v;
                    tmp_v.push_back(i);
                    tmp_v.push_back(pid_table[i][j]);
                    return_v = tmp_v;
                }
            }
        }
        return return_v;
    }
    // lru sample페이지 교체 알고리즘
    void lru_sampled_replacement(int pid, int process_id){
        bool replacement_succes = false;
        if(debug){
            cout << "--- reference byte ---\n";
            for(int i=0;i<reference_byte.size();i++){
                cout << "process " << i << endl;
                for(int j=0;j<reference_byte[i].size();j++){
                    cout <<"pid " << j << " | ";
                    reference_byte[i][j].print_byte();
                }
            }
        }
        while(!replacement_succes){
            vector<int> on_pm_aid_list = get_on_pm_aid_list();
            vector<int> used_val;
            for(int i=0;i<on_pm_aid_list.size();i++){
                vector<int> page_info_v = uid_to_processid_pageid(on_pm_aid_list[i]);
                cout << "translated pid : " << page_info_v[1] <<endl;
                used_val.push_back(reference_byte[page_info_v[0]][page_info_v[1]].get_sum());
            }
            int min_byte_sum = pow(2,10);
            int min_uid; // victim
            for(int i=0;i<used_val.size();i++){
                if(used_val[i]< min_byte_sum){
                    min_uid = on_pm_aid_list[i];
                    min_byte_sum = used_val[i];
                }
                if(used_val[i] == min_byte_sum && min_uid > on_pm_aid_list[i]){
                    min_uid = on_pm_aid_list[i];
                    min_byte_sum = used_val[i];
                }
            }
            if(debug){
                for(int i=0;i<on_pm_aid_list.size();i++){
                    cout <<"pid : "<< process_id << ", aid : " << on_pm_aid_list[i] << " , count : "<<used_val[i] << endl;
                }
                cout <<"------\n";
                for(int i=0;i<reference_byte.size();i++){
                    cout <<"process " << i <<endl;
                    for(int j=0;j<reference_byte[i].size();j++){
                        reference_byte[i][j].print_byte();
                    }
                }
            }
            release_aid(min_uid);
            merge_buddy();
            replacement_succes = allocate_aid(pid, process_id);
        }
    }
    // lru 페이지 교체 알고리즘
    void lru_replacement(int pid, int process_id){
        bool replacement_success = false;
        int count_it = 0;
        while(!replacement_success){
            count_it ++ ;
            
            int count_used = 0;
            vector<int> on_pm_aid_list = get_on_pm_aid_list();
            vector<int> used_aid((aid_count),0);
            for(int i=0;i<used_aid.size();i++){
                // 현재 물리 메모리에 있지 않은 aid 거르기
                if(count(on_pm_aid_list.begin(),on_pm_aid_list.end(),i)<=0){
                    used_aid[i] = 1;
                    count_used ++;
                }
            }
            if(debug){

                cout <<"--- used stack ---\n";
                for(int i=0;i<used_stack.size();i++){
                    cout << used_stack[i] << " ";
                }
                cout << endl;
                cout << "count : " << count_used << " to : " << aid_count-1 << endl;
            }
            for(int i=used_stack.size()-1;i>=0;i--){
                if(count_used == aid_count-1){
                        break;
                    }
                if(used_aid[used_stack[i]]==0){
                    count_used ++;
                    used_aid[used_stack[i]] = 1;
                    
                }
                if(debug){
                    cout << "aid : " << used_stack[i] << " count : "<< used_aid[used_stack[i]] << endl;

                }
            }
            // victime을 찾는다
            int need_to_be_replaced;
            for(int i=0;i<used_aid.size();i++){
                if(used_aid[i]==0){
                    need_to_be_replaced = i;
                    break;
                }
            }
            if(debug){
                cout << "victim aid : " << need_to_be_replaced << endl;
            }
            release_aid(need_to_be_replaced);
            merge_buddy();
            replacement_success = allocate_aid(pid, process_id);

        }



    }
    // clock 페이지 교체 알고리즘
    void clock_replacement(int pid, int process_id){
        bool replacement_success = false;
        while(!replacement_success){
            vector<int> on_pm_aid_list = get_on_pm_aid_list();
            sort(on_pm_aid_list.begin(),on_pm_aid_list.end());
            if(debug){
                cout << "--- sorted aid list ---\n";
                for(int i=0;i<on_pm_aid_list.size();i++){
                    cout << on_pm_aid_list[i] << " ";
                }
                cout << endl;

            }
            vector<vector<int>> aid_to_p_pair;
            for(int i=0;i<on_pm_aid_list.size();i++){
                aid_to_p_pair.push_back(uid_to_processid_pageid(on_pm_aid_list[i]));
            }
            // clock point sync
            int clock_aid_point;
            if(clock_idx_point == -1){
                clock_aid_point = on_pm_aid_list[0];
                clock_idx_point = 0;
            }
            else{
                clock_aid_point = on_pm_aid_list[clock_idx_point];
            }
            cout << "clock point aid : " << clock_aid_point << endl;
            // process to find victim
            bool found_victim = false;
            while(!found_victim){
                // 1. check current reference bit
                // 2. if ref bit == 0, found victim
                //    else ref bit == 1, set ref bit to 0, and proceed clock pointer
                int target_process_id = aid_to_p_pair[clock_idx_point][0];
                int target_page_id = aid_to_p_pair[clock_idx_point][1];
                int target_refbit;
                for(int i=0;i<pid_table[target_process_id].size();i++){
                    if(pid_table[target_process_id][i] == target_page_id){
                        target_refbit = reference_bit[target_process_id][i];
                        break;
                    }
                }
                if(target_refbit==0){
                    found_victim = true;
                    if(debug){
                        cout << "victim aid : " << clock_aid_point << endl; 
                    }
                    release_aid(clock_aid_point);
                }
                else{
                    // reference_bit[aid_to_p_pair[clock_idx_point][0]][aid_to_p_pair[clock_idx_point][1]] = 0;
                    // set all refernce bit of the page to zero
                    for(int i=0;i<pid_table[target_process_id].size();i++){
                        // for pid table value equals to target page number
                        if(pid_table[target_process_id][i]==target_page_id){
                            reference_bit[target_process_id][i] = 0;
                        }
                    }
                    clock_idx_point ++;
                    if(clock_idx_point == on_pm_aid_list.size()){
                        clock_idx_point = 0;
                    }
                    clock_aid_point = on_pm_aid_list[clock_idx_point];
                }
            }
            merge_buddy();
            replacement_success = allocate_aid(pid, process_id);

            
        }
    }
    // release하고 난 뒤 빈공간 merge
    void merge_buddy(){
        
        bool changed = true;
        // 더이상 합칠것이 없을 때까지 merge 진행한다.
        while (changed)
        {
            changed = false;
            print_segment_size();
            // 비어있는 버디들을 찾는다
            vector<int> segment_occupied;
            int starting_index_count = 0;
            for(int i=0;i<pm_segment_size.size();i++){
                cout << starting_index_count << endl;
                if(pm_table[starting_index_count]==-1){
                    segment_occupied.push_back(0);
                }
                else{
                    segment_occupied.push_back(1);
                }
                starting_index_count += pm_segment_size[i];
            }
            starting_index_count = 0;
            vector<int> need_to_be_merged(segment_occupied.size(),-1);
            int merge_count = 0;
            for(int i=0;i<pm_segment_size.size();i++){
                cout << "segment " << i << " occupied " << segment_occupied[i] << " size " << pm_segment_size[i] << " | "<< (starting_index_count/pm_segment_size[i])%2<<endl; 
                starting_index_count += pm_segment_size[i];

            }
            starting_index_count = 0;
            for(int i=0;i<pm_segment_size.size()-1;i++){
                // 왼쪽 버디 오른쪽 버디 둘다 비어있고, 왼쪽 버디와 오른쪽 버디가 사이즈가 동일하고, 현재 버디가 왼쪽 버디일때
                if((segment_occupied[i]==0)&&(segment_occupied[i+1]==0)&&(pm_segment_size[i]==pm_segment_size[i+1])){ // left buddy
                    if((starting_index_count/pm_segment_size[i])%2==0){
                        changed = true;
                        need_to_be_merged[i] = merge_count;
                        need_to_be_merged[i+1] = merge_count;
                    }
                    

                }
                starting_index_count += pm_segment_size[i];
            }

            // merge buddy
            vector<int> tmp_merge;
            for(int i=0;i<segment_occupied.size();i++){
                if(need_to_be_merged[i]!= -1){
                    tmp_merge.push_back(pm_segment_size[i]*2);
                    i++;
                }
                else{
                    tmp_merge.push_back(pm_segment_size[i]);
                }
            }
            pm_segment_size = tmp_merge;
        }
        

        
    }
};


// MQS스케줄러를 구현한 클래스이다.
class MQS {
public:
    vector <queue <Process>> subscheduler;
    vector<int> timequantum_list;
    MQS() {
        for (int i = 0;i < 10;i++) {
            queue<Process> q;
            subscheduler.push_back(q);
            if (i >= 5) {
                timequantum_list.push_back(TIME_QUANTUM);
            }
        }
        cout << "subshcheduler amout : " << subscheduler.size() << endl;
    }
    Process get_next_process() {
        bool found_nonempty = false;
        int found_q = -1;
        Process return_process;
        for (int i = 0; i < 10; i++) {
            if (!subscheduler[i].empty()) {
                found_nonempty = true;
                found_q = i;
                break;
            }
        }
        if (found_nonempty) {
            return_process = subscheduler[found_q].front();
            subscheduler[found_q].pop();

            // 해당 rr스케쥴러에서 프로세스가 실행될것이라면 time quantum을 초기화해줌
            if(found_q>= 5){
                timequantum_list[found_q-5] = TIME_QUANTUM;
            }
            scheduled = true;
            if(debug){
                cout << return_process.process_name << " is scheduled\n";
            }
            return return_process;

        }
        cout << "No Process selected" << endl;
        scheduled = false;
        return return_process;
    }
    void push_process(Process p) {

        if (p.priority <= 4) {
            cout << "process " << p.process_name << " will be pushed into "<< p.priority<< "  fcfs queue\n";
            subscheduler[p.priority].push(p);
            cout << "pushing success\n";



        }
        else {
            cout << "process " << p.process_name << " will be pushed into "<< p.priority <<  " rr queue\n";
            subscheduler[p.priority].push(p);

            cout << "pushing success\n";

        }
    }
    void print_scheduler(FILE*out){
        for(int i=0;i<10;i++){
            // cout << "subscheduler "<< i << " | ";
            fprintf(out,"RunQueue %d: ", i);
            if(subscheduler[i].empty()){
                fprintf(out,"Empty");
            }
            else{
                queue<Process> tmp;
                // if(debug){
                //     cout << "스케줄러 " << i << " size : " << subscheduler[i].size() << endl;
                //  }
                int scd_size = subscheduler[i].size();
                for(int j=0;j<scd_size;j++){
                    fprintf(out,"%d(%s) ", subscheduler[i].front().pid, (char*)subscheduler[i].front().process_name.c_str());
                    tmp.push(subscheduler[i].front());
                    subscheduler[i].pop();
                    total_waiting_count ++ ;
                }
                for(int j=0;j<scd_size;j++){
                    subscheduler[i].push(tmp.front());
                    tmp.pop();
                }

            }
            fprintf(out,"\n");
        }
    }


};


void readInput(MQS scheduler) {
    cout << "input_directory " << input_dir << endl;
    string inputFileDir = input_dir + "/input";
    cout << inputFileDir << endl;
    ifstream File((char*)inputFileDir.c_str());
    File >> total_event_num >> vm_size >> pm_size >> page_size;
    cout << total_event_num << " " << vm_size << " " << pm_size << " " << page_size << endl;
    string p_name;
    int cpu_time, shcheduler_num, process_count;
    process_count = 0;
    for (int i = 0;i < total_event_num; i++) {

        File >> cpu_time >> p_name >> shcheduler_num;
        cout << cpu_time << " " << p_name << " " << shcheduler_num << endl;
        if (p_name == "INPUT") {
            vector<int> io;
            // 5 Input 1
            // cpu cycle / input / pid
            io.push_back(cpu_time);
            io.push_back(shcheduler_num);
            will_input_list.push_back(io);
            // cpu cycle , pid 리스트를 저장해놓는다

        }
        else {
            Process newprocess = Process(process_count, (input_dir + "/" + p_name), p_name, (vm_size / page_size), shcheduler_num, cpu_time);
            process_count++;
            total_process.push_back(newprocess);
            max_cycle_process = max(max_cycle_process,cpu_time);


        }
    }
    // File.close();

}
void print_current_cycle_info(FILE* out,int cycle_count, string function, Process running_process, bool running_process_exist, bool currently_scheduled){
    fprintf(out, "[%d Cycle] Scheduled Process: ", cycle_count);
  
    if(currently_scheduled){
        fprintf(out, "%d %s (priority %d)\n", running_process.pid, (char*)running_process.process_name.c_str(), running_process.priority);
    }
    else{
        fprintf(out, "None\n");
    }
    if(debug){
        cout << "pname " << running_process.process_name << endl;
        cout << "code it " << running_process.code_iterator<<endl;
    }
    fprintf(out, "Running Process: ");
    cout << "["<<cycle_count << " cycle]\n";
    if (running_process_exist){
        fprintf(out, "Process#%d(%d) running code %s line %d(op %d, arg %d)\n", running_process.pid, running_process.priority, (char*)running_process.process_name.c_str(), running_process.code_iterator+1, running_process.codes[running_process.code_iterator][0], running_process.codes[running_process.code_iterator][1]);

    }
    else{

        fprintf(out, "None\n");
    }
    
}
// io wait list를 출력하는 함수이다.
void print_iowait(FILE*out, vector<Process> iowait_list){
    fprintf(out, "IOWait List: ");
    if(iowait_list.empty()){
        fprintf(out, "Empty");
    }
    else{
        for(int i=0;i<iowait_list.size();i++){
            fprintf(out, "%d(%s) ", iowait_list[i].pid,(char*)iowait_list[i].process_name.c_str());
        }
    }
    fprintf(out, "\n");
    // 다음 사이클과 구분
    fprintf(out, "\n");

}
// sleep중인 프로세스를 출력하는 함수이다.
void print_sleep(FILE*out, vector<Process> sleep_list){
    fprintf(out, "SleepList: ");
    if(sleep_list.empty()){
        fprintf(out, "Empty");
    }
    else{
        for(int i=0;i<sleep_list.size();i++){
            fprintf(out, "%d(%s) ", sleep_list[i].pid, (char*)sleep_list[i].process_name.c_str());
        }
    }
    fprintf(out, "\n");

}
// void print_scheduler();
int main(int argc, char** argv) {
    getcwd(tmp, 256);
    for (int i = 1; i < argc; i++) {
        string option_label, option_value;
        string determ = "=";
        string option = string(argv[i]);
        option_label = option.substr(1, option.find(determ) - 1);
        option_value = option.substr(option.find(determ) + 1);
        cout << "option " << i << " " << option_label << " " << option_value << endl;
        if (option_label == "page") {
            page = option_value;
        }
        if (option_label == "dir") {
            input_dir = option_value;
            cout << "input directory : " << input_dir << endl;
        }
        if(option_label == "debug"){
            if(option_value == "false"){
                debug = false;
            }
            else{
                debug = true;
            }
        }

    }
    input_directory = input_dir;
    vector<int> op_info(4);
    MQS scheduler = MQS(); // cycle에 사용될 main scheduler
    bool running_process_exist = false; // 현재 실행중인 프로세스가 있는지 없는지 bool flag
    Process running_process; // 현재 실행중인 프로세스
    readInput(scheduler); // input file들 읽어오는 함수 실행
    // process들을 cpu time에 따라 정리하여 push
    for(int i=0;i<=max_cycle_process;i++){
        vector<Process> process_list_at_cycle;
        // cout << "cycle time " << i << endl;
        for(int j=0;j<total_process.size();j++){
            if(total_process[j].cycle_time == i){
                process_list_at_cycle.push_back(total_process[j]);
                // if(debug){
                //     cout << total_process[j].process_name << "at cpu time " << total_process[j].cycle_time << endl;
                // }
            }

        }
        process_list_at_cpu_cycle.push_back(process_list_at_cycle);
    }
    VMSystem vmsystem = VMSystem(pm_size, vm_size, page_size, total_process.size());
    vmsystem.print_segment_size();
    int cycle_count = 0; // cycle count number
    int allocation_id = 1; // alocation id (auto increment)
    string function; // current functin name
    // 출력 파일들 생성
    string memory_out_dir = input_dir +"/memory.txt";
    char* char_memory_out = (char*) memory_out_dir.c_str();
    FILE* MemoryOut = fopen(char_memory_out, "w");
    string scheduler_out_dir = input_dir + "/scheduler.txt";
    cout << "스케쥴러 출력 : " << scheduler_out_dir << endl;
    char* char_scheduler_out = (char*) scheduler_out_dir.c_str();
    FILE* SchedulerOut = fopen(char_scheduler_out, "w");
    vector<Process> on_handling;
    // read from input file and make proccess and push them into scheduler
    while (true) {
        if(debug){
            cout << "is sleep list empty " << sleep_list.empty() << endl;
            cout << "end process count : " << end_process_count << endl;

        }
        if(end_process_count == total_process.size()){
            
            break;
        }
        // 자고 있는 프로세스가 잠에서 깰 차례인지 확인하고 깨운다
        if (!sleep_list.empty()) {
            for (int i = 0; i < sleep_list.size(); i++) {
                if (sleep_list[i].sleeping_time_left == 1)// 프로세스가 쿨쿨 자고 있을 때
                {
                    scheduler.push_process(sleep_list[i]);
                    for (int j = i;j < sleep_list.size() - 1;j++) {
                        sleep_list[j] = sleep_list[j + 1];
                    }
                    sleep_list.pop_back();
                }
                else{
                    sleep_list[i].sleeping_time_left --;
                }
            }
        }

        // io 작업해서 io를 기다리고 있는 프로세스 들을 처리해준다.
        for(int i=0; i<will_input_list.size(); i++){
            
            if(will_input_list[i][0] == cycle_count){
                cout << "*********new io\n";
                for(int j=0;j<iowait_list.size();j++){
                    if(will_input_list[i][1] == iowait_list[j].pid){ 
                        // pid에 해당하는 프로세스 찾음
                        scheduler.push_process(iowait_list[j]);
                        if(debug){
                            cout << iowait_list[j].process_name << " 프로세스가 io작업이 처리되었습니다.\n";
                        }
                        // io작업을 처리해준 뒤 iowait list를 한칸씩 땡겨준다
                        // 빈칸이 남으면 안되기 때문
                        for(int k=j;k<iowait_list.size()-1;k++){
                            iowait_list[k] = iowait_list[k+1];
                        }
                        iowait_list.pop_back();
                        break;


                    }
                }
            }
        }
        if(debug){
            cout << "input list\n";
            for(int i=0;i<will_input_list.size();i++){
                for(int j=0;j<will_input_list[i].size();j++){
                    cout << will_input_list[i][j] << " ";
                }
                cout << endl;
            }
            if(cycle_count == 60){
                break;
            }
        }
        if(debug){
            cout << "io 작업이 완료되었습니다\n";
        }
        // 해당 cpu cycle에 스케쥴러에 push되어야할 process들을 찾아 스케쥴러에 넣어준다
        if(cycle_count < process_list_at_cpu_cycle.size()){
            for(int i=0;i<process_list_at_cpu_cycle[cycle_count].size();i++){
                scheduler.push_process(process_list_at_cpu_cycle[cycle_count][i]);
                on_handling.push_back(process_list_at_cpu_cycle[cycle_count][i]);
            }

        }
        
        int page_id_info;
        // 현재 실행되고 있는 프로세스가 없다면 스케쥴러에서 뽑는다.
        // 더이상 진행할 필요가 없으면 page fault개수를 출력하고 끝낸다.
        bool currently_scheduled = false;
        if (!running_process_exist) {
            running_process = scheduler.get_next_process();
            if(scheduled){
                currently_scheduled = true;
                running_process_exist = true;
                scheduled = false;
                if(debug){
                    cout << "이번에 실행될 프로세스는 "<< running_process.process_name<< " (pid "<< running_process.pid << ")" <<" 입니다." << endl;

                }
            }
            else{
                if(debug){
                    cout << "스케쥴링된 프로세스가 없습니다.\n";
                }
                if(sleep_list.empty() && iowait_list.empty() && (cycle_count>=max_cycle_process)){
                    break;
                }
            }
        }
        cout << "time quantum : " << scheduler.timequantum_list[1] <<endl;
        
         // 정보 프린트
        print_current_cycle_info(SchedulerOut,cycle_count, function, running_process, running_process_exist, currently_scheduled); // line 1,2
        cout << "** stack ** \n";
        for(int i=0;i<vmsystem.used_stack.size();i++){
            cout << vmsystem.used_stack[i] << " ";
        }
        cout << endl;
        scheduler.print_scheduler(SchedulerOut); // line 3
        // reference byte update
        if(page == SAMPLED_LRU){
            if(cycle_count%8 == 0 && cycle_count > 0){
                cout << "---updated reference byte---\n";
                for(int i=0;i<vmsystem.reference_byte.size();i++){
                    cout << "process " << i << endl;
                    for(int j=0;j<vmsystem.reference_byte[i].size();j++){
                        cout << "pid " << j << " | ";
                        vmsystem.reference_byte[i][j].update();
                        vmsystem.reference_byte[i][j].print_byte();
                    }
                }
            }
            cout << "---saved---\n";
            for(int i=0;i<vmsystem.reference_byte.size();i++){
                    cout << "process " << i << endl;
                    for(int j=0;j<vmsystem.reference_byte[i].size();j++){
                        cout << "pid " << j << " | ";
                        vmsystem.reference_byte[i][j].print_saved();
                    }
                }
            // reference bit update
            for(int i=0;i<vmsystem.reference_bit.size();i++){
                for(int j=0;j<vmsystem.reference_bit[i].size();j++){
                    vmsystem.reference_bit[i][j] = 0;
                }
            }
        }
        if(running_process_exist){
            op_info = running_process.get_line();
            if(running_process.priority>=5){

                scheduler.timequantum_list[running_process.priority-5] --;
            }
            int current_pid = running_process.pid;
            //  0: Memory Allocation
            //  1: Memory Access
            //  2: Memory Release
            //  3: Non-memory instruction
            //  4: Sleep
            //  5: IOwait
            // ALLOCATION, ACCESS, RELEASE, NON-MEMORY, SLEEP, IOWAIT, NO-OP
            switch (op_info[0])
            {
            case 0: // allocation
            {
                function = ALLOCATION;
                if(debug){
                    // cout << "code : " << op_info[0] << " " << op_info[1] << endl;
                    cout << "allocation\n";
                }
                int pid_empty_start;
                for(int i=0;i<vmsystem.pid_table[current_pid].size();i++){
                    if(vmsystem.pid_table[current_pid][i]==-1){
                        pid_empty_start = i;
                        break;
                    }
                }
                vmsystem.page_num_table[running_process.pid].push_back(op_info[1]);
                for(int i=0;i<op_info[1];i++){
                    vmsystem.pid_table[current_pid][i+pid_empty_start] = vmsystem.pid_count[current_pid];
                    vmsystem.valid_bit[current_pid][i+pid_empty_start] = 0;
                }

                ByteArray nbyte = ByteArray();

                vmsystem.reference_byte[current_pid].push_back(nbyte);
                cout << "Byte 크기 : " << vmsystem.reference_byte[current_pid].size() << endl;
                // cout << "point 1\n";
                if(page==SAMPLED_LRU){
                    vmsystem.reference_byte[0][0].print_saved();

                }
                page_id_info = vmsystem.pid_count[current_pid];
                vmsystem.pid_to_aid[current_pid].push_back(-10);
                vmsystem.pid_count[current_pid]++;
                // cout << "point 2\n";


                break;
            }
            case 1: // access
            {
                function = ACCESS;
                cout << "pid : " << running_process.pid << " page : " << op_info[1] << endl;
                bool page_exist_in_pm = false;
                page_id_info = op_info[1];
                for(int i=0;i<vmsystem.pm_table.size();i++){

                    if(vmsystem.pid_to_aid[current_pid][op_info[1]] == vmsystem.pm_table[i]){
                        page_exist_in_pm = true;
                        break;
                    }
                }
                // 이미 pm 위에 page가 존재하여 추가적인 aid 할당 필요 없는 경우
                if(page_exist_in_pm){
                    if(debug){
                        cout << "페이지 할당이 필요없습니다.\n";
                    }
                    for(int i=0;i<vmsystem.pid_table[current_pid].size();i++){
                        if(vmsystem.pid_table[current_pid][i]==op_info[1]){
                            vmsystem.reference_bit[current_pid][i] = 1;
                        }
                    }
                   
                    int target_idx;
                    for(int i=0;i<vmsystem.pid_table[running_process.pid].size();i++){
                        if(vmsystem.pid_table[running_process.pid][i]==op_info[1]){
                            target_idx = i;
                            break;
                        }
                    }
                    vmsystem.used_stack.push_back(vmsystem.aid_table[running_process.pid][target_idx]);
                }
                else{
                    total_page_fault ++;
                    bool access_result = vmsystem.allocate_aid(op_info[1], running_process.pid);
                    if(!access_result){
                        if(debug){
                            cout << "페이지 교체가 필요합니다.\n";
                        }
                        if(page == LRU){
                            cout << "LRU page replacement start\n";
                            vmsystem.lru_replacement(op_info[1], running_process.pid);
                        }
                        if(page == SAMPLED_LRU){
                            cout << "SAMPLED LRU page replacement start\n";
                            vmsystem.lru_sampled_replacement(op_info[1],running_process.pid);
                        }
                        if(page == CLOCK){
                            cout << "CLOCK page replacement start\n";
                            vmsystem.clock_replacement(op_info[1], running_process.pid);
                        }
                    }
                    for(int i=0;i<vmsystem.pid_table[current_pid].size();i++){
                        if(vmsystem.pid_table[current_pid][i]==op_info[1]){
                            vmsystem.reference_bit[current_pid][i] = 1;
                        }
                    }
                }

                break;
            }
            case 2: // release
                function = RELEASE;
                page_id_info = op_info[1];
                vmsystem.release_page(page_id_info, running_process.pid);
                if(debug){
                    cout << "release\n";
                }
                break;
            case 3: // non-memory
                function = NON_MEMORY;
                if(debug){
                    cout << "non-memory\n";
                }
                break;
            case 4: // sleep
                function = SLEEP;
                running_process.sleeping_time_left = op_info[1];
                if(op_info[2]!=0){
                    sleep_list.push_back(running_process);
                }
                else{
                    end_process_count ++;
                    for(int i=0;i<on_handling.size();i++){
                        if(on_handling[i].pid == running_process.pid){
                            on_handling[i].end = true;
                        }
                    }
                    vmsystem.remove_process_from_pm(running_process.pid);


                }
                
                running_process_exist = false;
                if(debug){
                    cout << "sleep\n";
                }
                break;
            case 5: // iowait
                function = IOWAIT;
                if(op_info[2]!= 0){
                    iowait_list.push_back(running_process);
                }
                else{
                    end_process_count ++ ;
                    for(int i=0;i<on_handling.size();i++){
                        if(on_handling[i].pid == running_process.pid){
                            on_handling[i].end = true;
                        }
                    }
                    vmsystem.remove_process_from_pm(running_process.pid);
                }
                running_process_exist = false;
                if(debug){
                    cout << "iowait\n";
                }
                break;
            default:
                if(debug){
                    cout << "unsupported operation\n";
                }
                break;
            }
        }
        else{
            function = NO_OP;
            cout <<"no-op\n";
        }
        
        // reference byte 
        if(page == SAMPLED_LRU){
            for(int i=0;i<vmsystem.reference_byte.size();i++){
                cout << "process " << i << endl;
                for(int j=0;j<vmsystem.reference_byte[i].size();j++){
                    int current_bit;
                    for(int k=0;k<vmsystem.pid_table[i].size();k++){
                        if(vmsystem.pid_table[i][k] == j){
                            current_bit = vmsystem.reference_bit[i][k];
                            break;
                        }
                    }
                    cout << current_bit << endl;

                    vmsystem.reference_byte[i][j].push_bit(current_bit);
                }
            }
        }


        vmsystem.print_memory_data(MemoryOut, cycle_count, function, running_process, page_id_info, on_handling);
        // 프로세스의 코드를 다 실행시키고 나면 process를 종료한다
        if(running_process_exist){

            if(running_process.codes.size() == running_process.code_iterator){
                running_process.end = true;
                running_process_exist = false;
                end_process_count ++;
                for(int i=0;i<on_handling.size();i++){
                        if(on_handling[i].pid == running_process.pid){
                            on_handling[i].end = true;
                        }
                    }
                vmsystem.remove_process_from_pm(running_process.pid);
                if(debug){
                    cout << running_process.process_name << " 수행이 끝났습니다\n";
                }
            }
        }
        // round robin scheduler의 경우 time quantum을 다 썼으면 다시 스케쥴러에 넣어준다.
        if(running_process_exist){

            if(running_process.priority >= 5){
                if(scheduler.timequantum_list[running_process.priority-5]==0){
                    scheduler.push_process(running_process);
                running_process_exist = false;

                }

            }
        }
       
        print_sleep(SchedulerOut,sleep_list); // line 4
        print_iowait(SchedulerOut,iowait_list); // line 5
        scheduled = false;

        cycle_count ++;
    }
    fprintf(MemoryOut, "page fault = %d\n", total_page_fault);
    if(debug){
        cout << "total waiting : " << total_waiting_count << endl;
    }
    


}



