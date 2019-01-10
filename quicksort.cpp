


#include "quicksort.h"


#include <iostream>

#include <sstream>

#include <set>
#include <vector>


#include <boost/bind.hpp>



#include "muduo/net/EventLoop.h"
#include "muduo/net/EventLoopThread.h"
#include "muduo/net/EventLoopThreadPool.h"

#include "muduo/base/Logging.h"

#include "tst_1.h"


#include <future>
#include <thread>


using namespace std;
using namespace muduo;
using namespace muduo::net;


extern void print_vect_int( const std::vector<int>& array_nums );
extern void print_arra_int( int array_nums[], int ilen ) ;





/*********************************** 随机化快速排序算法实现 ****************************************/
// 对arr[left....right]进行partition操作
// 返回p，使得arr[left, p-1] < arr[p]，arr[p+1, right] > arr[p]
template < typename T >
int __partition( T array[ ], int left, int right ) {
    T   v = array [ left ];// 将第一个元素作为"基准"元素
    int j = left;// 将<v部分的分界点位置j初始化为本序列中的第一个元素位置（也就是<v部分的最后一个元素位置）

    for ( int i = left + 1; i <= right; i ++ ) {
        if ( array[i] < v ) {
            j ++;
            if ( j != i ) {
                printf( "j=%d i=%d array[j]=%d array[i]=%d\n", j, i, array[j], array[i] );
                print_arra_int( array, right + 1 );
                std::swap( array[j], array[i] );// 这里采用了另一种那写法，因为j++之后指向的就是>v部分的第一个元素，交换位置之后其实类似于将>v的部分整体往右边移动了一个位置
            }
//            printf( "j=%d i=%d array[j]=%d array[i]=%d\n", j, i, array[j], array[i] );
//            print_arra_int( array, right + 1 );
        }
    }

    printf( "j=%d left=%d\n", j, left );
    std::swap(array[left], array[j]);   // 遍历完成之后只需要将"基准"元素（也就是第一个元素）与当前j指向的位置交换位置即可
    return j;                       // 因为"基准"元素并不属于<v的部分，所以交换之后此时j指向的元素就是"基准"元素
}
// 对arr[left...right]范围内数组序列进行快速排序操作
template<typename T>
void __quickSort (T arr[], int left, int right)
{
    int p = __partition<T>(arr, left, right);  // 对arr[left...right]区间元素进行partition操作，找到"基准"元素
//    printf( "p=%d\n", p );
    if (left < right) {
        __quickSort<T>(arr, left, p - 1);          // 对基准元素之前的序列递归调用__quickSort函数
        __quickSort<T>(arr, p + 1, right);         // 对基准元素之后的序列递归调用__quickSort函数
    } else {
        if ( p == 0 )
            printf( "left=%d rigtht=%d\n", left, right );
    }
}

template<typename T>
void quickSort (T arr[], int count) {
    __quickSort<T>(arr, 0, count - 1);   // 调用__quickSort函数进行快速排序
}
/********************************************************************************************/

void tst_quick_sort() {

//    int in_array[ ] = { 100, 27, 36, 101, 12334, 1024, 77891 };
    int in_array[ ] = { 100, 101, 120, 30 };

    int size_array = ARRAY_SIZE(in_array);

    print_arra_int( in_array, size_array );

    quickSort<int>( in_array, size_array );

    print_arra_int( in_array, size_array );


}





//标准的快排方法
int partion(int a[],int left,int right)//首先进行分区操作
{
    int flag = a[left];//将首元素当作标杆，选择最右边元素最为标杆时代码会有略微不同
    int j = left;
    for (int i = left+1; i <= right; ++i)//首先忽略首元素
    {
        if (a[i] < flag)
        {
            j++;
            if ( j != i)
            {
                std::swap(a[i],a[j]);//将标杆数一直往右移动，遇到比标杆数小的就交换到从头开始的位置上
            }
        }
    }
    std::swap(a[j],a[left]);//j位置的元素和首元素互换，即可保证标杆元素左边为小于等于，右边为大于等于
    return j;
}

//取区间内随机数的函数
int Rand(int low, int high)
{
    int size = high - low + 1;
    int ret  = low + rand()%size;

//    printf( "rand_ret=%d\n", ret );
    return ret;
}
int partion1(int a[], int left, int right)//首先进行分区操作
{
    std::swap(a[Rand(left, right)], a[left]);//随机选择区间的一个数,将其与首元素进行交换
    int flag = a[left];//将首元素当作标杆
    int j    = left;
    for (int i = left+1; i <= right; ++i)//首先忽略首元素
    {
        if (a[i] < flag)
        {
            j++;
            if (j != i) {
                std::swap(a[i], a[j]);//将标杆数一直往右移动，遇到比标杆数小的就交换到从头开始的位置上
            } else {
                printf("j==i val=%d\n", j);
            }
        }
    }
    std::swap(a[j], a[left]);//j位置的元素和首元素互换，即可保证标杆元素左边为小于等于，右边为大于等于
    return j;
}
//标准递归版本的快速排序
void Fast_sort(int a[],int left,int right)
{
    if (left < right)
    {
        int index = partion1(a, left, right);//partion也可以
//        printf( "index=%d\n", index );
        print_arra_int( a, right+1 );

        Fast_sort(a, left,    index-1);
        Fast_sort(a, index+1, right);
    }
}
void tst_quick_sort_1() {
    srand((unsigned)time(0));

//    int a[10] = {12,45,748,12,56,3,89,4,48,2};
    int a[4] = {100, 101, 120, 30};
    std::cout << "sizeof( a )=" << sizeof( a ) << std::endl;

    print_arra_int( a, sizeof(a) / sizeof(int) );

    Fast_sort(a, 0, sizeof(a) / sizeof(int) );

    print_arra_int( a, sizeof(a) / sizeof(int) );

//    for (size_t i = 0; i != 10; ++i)
//    {
//        cout<<a[i]<<" ";
//    }
//    cin.get();

}








using namespace std;
int arrs[] = { 23, 65, 12, 3, 8, 76, 345, 90, 21, 75, 34, 61/*, 110, 32, 77, 141*/ };
int arrLen = sizeof(arrs) / sizeof(arrs[0]);


void adjustHeap(int* arrs, int p, int len){
    int curParent = arrs[p];
    int child = 2*p + 1;   //左孩子

    while(child < len){     //没有孩子
        if(child+1<len&&arrs[child]<arrs[child+1]){
            child++;    //较大孩子的下标
        }
        if(curParent<arrs[child]){
            arrs[p]=arrs[child];
            //没有将curParent赋值给孩子是因为还要迭代子树，
            //如果其孩子中有大的，会上移，curParent还要继续下移。
            p=child;
            child=2*p+1;
        }
        else
            break;
    }
    arrs[p]=curParent;
}

void heapSort(int arrs[], int len){
    int ilastlevel = arrLen/2 - 1;
    printf( "arrs[%d]=%d\n", ilastlevel, arrs[ilastlevel] );
    printf( "len=%d\n", len );

    for (int i = arrLen/2 - 1; i >= 0; --i)//从最后一个非叶子节点的父结点开始建堆
        adjustHeap(arrs, i, arrLen);

    for (int j = arrLen - 1; j >= 0; --j)//j表示数组此时的长度，因为len长度已经建过了，从len-1开始
    {
        std::swap(arrs[0], arrs[j]);//交换首尾元素,将最大值交换到数组的最后位置保存
        adjustHeap(arrs, 0, j);//去除最后位置的元素重新建堆，此处j表示数组的长度，最后一个位置下标变为len-2
    }
}

int tst_heapsort_fun_1()
{
    print_arra_int( arrs, arrLen );

    heapSort(arrs, arrLen);

    print_arra_int( arrs, arrLen );
    return 0;
}



/*
 * (最大)堆的向下调整算法
 *
 * 注：数组实现的堆中，第N个节点的左孩子的索引值是(2N+1)，右孩子的索引是(2N+2)。
 *     其中，N为数组下标索引值，如数组中第1个数对应的N为0。
 *
 * 参数说明：
 *     a -- 待排序的数组
 *     start -- 被下调节点的起始位置(一般为0，表示从第1个开始)
 *     end   -- 截至范围(一般为数组中最后一个元素的索引)
 */
void maxHeapDown(int* a, int start, int end)
{
    int c = start;            // 当前(current)节点的位置
    int l = 2*c + 1;          // 左(left)孩子的位置
    int tmp = a[c];           // 当前(current)节点的大小
    for (; l <= end; c=l, l=2*l+1)
    {
        // "l"是左孩子，"l+1"是右孩子
        if (l < end && a[l] < a[l+1])
            l++;        // 左右两孩子中选择较大者，即m_heap[l+1]
        if (tmp >= a[l])
            break;        // 调整结束
        else            // 交换值
        {
            a[c] = a[l];
            a[l] = tmp;
//            std::swap(tmp, a[l]);
        }
    }
}
/*
 * 堆排序(从小到大)
 *
 * 参数说明：
 *     a -- 待排序的数组
 *     n -- 数组的长度
 */
void heapSortAsc(int* a, int n)
{
    int i, tmp;
    // 从(n/2-1) --> 0逐次遍历。遍历之后，得到的数组实际上是一个(最大)二叉堆。
    for (i = n/2 - 1; i >= 0; i--)
        maxHeapDown(a, i, n-1);

    // 从最后一个元素开始对序列进行调整，不断的缩小调整的范围直到第一个元素
    for (i = n - 1; i > 0; i--)
    {
        // 交换a[0]和a[i]。交换后，a[i]是a[0...i]中最大的。
//        tmp = a[0]; a[0] = a[i]; a[i] = tmp;
        std::swap(a[0], a[i]);
        // 调整a[0...i-1]，使得a[0...i-1]仍然是一个最大堆。
        // 即，保证a[i-1]是a[0...i-1]中的最大值。
        maxHeapDown(a, 0, i-1);
    }
}
int tst_heapsort_fun_2()
{
    print_arra_int( arrs, arrLen );

    heapSortAsc(arrs, arrLen);

    print_arra_int( arrs, arrLen );
    return 0;
}

typedef multiset<int, greater<int> > itSet;
typedef multiset<int, greater<int> >::iterator setIterator;

void getLeastNumbers(const vector<int> &data, itSet &leastNumbers, int k){
    leastNumbers.clear();
    if(k < 1 || data.size() < k) return;
    for(vector<int>::const_iterator iter = data.begin(); iter != data.end(); iter++){
        if(leastNumbers.size() < k){
            leastNumbers.insert(*iter);
        }else{
            setIterator iterGreatest = leastNumbers.begin();
            if(*iter < *iterGreatest){
                leastNumbers.erase(iterGreatest);
                leastNumbers.insert(*iter);
            }
        }
    }
}

void my(const vector<int>& in_source, itSet& outSet, int k) {
    outSet.clear();
    if (k<1 || in_source.size() < k) return ;

    for ( vector<int>::const_iterator it = in_source.begin(); it != in_source.end(); ++ it ) {
        if ( outSet.size() < k )
            outSet.insert(*it);
        else {
//            for (setIterator setIt = outSet.begin(); setIt != outSet.end(); setIt ++)
//                std::cout << "setIt=" << *setIt << std::endl;
//            return ;
            setIterator iterGreatest = outSet.begin();
            if (*it < *iterGreatest) {
                outSet.erase(iterGreatest);
                outSet.insert(*it);
            }
        }
    }
}


class CTestAAA {
private:
    int m_ivalue;
public:
    CTestAAA() : m_ivalue(1024) {

    }
    int PrintValue(int inVal) {
        std::cout << "sumvalue=" << m_ivalue + inVal << std::endl;
    }
    void PrintTwoParam( int val1, int val2 ){
        std::cout << "val1=" << val1 << ", val2=" << val2 << std::endl;
    }
};

int tst_Top_k_1() {
//    std::set <int> int_set;
//    int_set.insert( 121 ); int_set.insert(10); int_set.insert( 17);int_set.insert(-9);
//    for(setIterator ittt = int_set.begin(); ittt != int_set.end(); ++ittt)
//        std::cout<< *ittt << " ";
//    std::cout << std::endl;

//    return 1;
    CTestAAA ttttA;
    boost::function< int(int) > bindFunction = boost::bind(&CTestAAA::PrintValue, &ttttA, _1);

    bindFunction(500);

    boost::function< void(int, int) > bind2_param = boost::bind(&CTestAAA::PrintTwoParam, &ttttA, _1, _2);
    bind2_param( 1024, 3096 );
    return 1;


    int A[] = {2, 1, 4, 2, 5, 1, 6, 9, 3, 4, 123, 654, 1098, 13, 6540 };
    vector<int> dataset(A, A + ARRAY_SIZE(A));
    itSet leastNumbersSet;
    int k = 7;

    getLeastNumbers(dataset, leastNumbersSet, k);

//    leastNumbersSet.count();
//    for(setIterator i = leastNumbersSet.begin(); i != leastNumbersSet.end(); ++i)
//        std::cout<< *i << " ";
//    std::cout << std::endl;

//    std::cout << "leastNumbersSet.count=" << leastNumbersSet.count() << std::endl;

    return 0;
}




class Person
{
public:
    Person(int v) {
        value = v;
        std::cout << "Cons" <<value<< std::endl;
    }
    ~Person() {
        std::cout << "Des" <<value<< std::endl;
    }

    int value;
};

int tst_SharedPtr_1()
{
    std::shared_ptr<Person> p1(new Person(1));// Person(1)的引用计数为1

    std::shared_ptr<Person> p2 = std::make_shared<Person>(2);

    p1.reset(new Person(3));// 首先生成新对象，然后引用计数减1，引用计数为0，故析构Person(1)
                            // 最后将新对象的指针交给智能指针

    std::shared_ptr<Person> p3 = p1;//现在p1和p3同时指向Person(3)，Person(3)的引用计数为2

    p1.reset();//Person(3)的引用计数为1
    p3.reset();//Person(3)的引用计数为0，析构Person(3)
    return 0;
}


void func(int inparam) {
    std::cout << "inparam=" << inparam << std::endl;
}
void C11Fun_1() {
    std::vector<int> arr;
    arr.push_back(1);
    arr.push_back(2);

//    for (auto it = arr.begin(); it != arr.end(); it++)
//    {
//         std::cout << *it << std::endl;
//    }

//    std::for_each(arr.begin(), arr.end(), func);

    for ( auto n : arr ) {
        std::cout << n << std::endl;
    }
}



class BitMap
{
public:
    BitMap(size_t range)
    {
        //此时多开辟一个空间
//        _bits.resize(range / 32 + 1);
        std::cout << "(range / 32 + 1)=" << (range / 32 + 1) << std::endl;
    }
    void Set(size_t x)
    {
        int index = x / 32;//确定哪个数据（区间）
        int temp = x % 32;//确定哪个Bit位
        _bits[index] |= (1 << temp);//位操作即可
    }
    void Reset(size_t x)
    {
        int index = x / 32;
        int temp = x % 32;
        _bits[index] &= ~(1 << temp);//取反
    }
    bool Test(size_t x)
    {
        int index = x / 32;
        int temp = x % 32;
        if (_bits[index]&(1<<temp))
            return 1;
        else
            return 0;
    }

private:
    vector<int> _bits;
};

void TestBitMap()
{
    printf( "-1 = 0X%0X\n", -1 );

    BitMap am(-1);
//    BitMap bm(200);
//    bm.Set(136);
//    bm.Set(1);
//    cout << bm.Test(136) << endl;
//    bm.Reset(136);
//    cout << bm.Test(136) << endl;
}









