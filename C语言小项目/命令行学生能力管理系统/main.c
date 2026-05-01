#include <stdio.h>
#include <string.h>

#define CAPACITY_LENGTH 3   // 能力长度，共有多少种能力

const char *CapacityName[] = {"攻击", "防御", "血量"};  // 学生能力列表，对应学生能力枚举

typedef enum    // 学生能力枚举。0=攻击 1=防御 2=血量
{
    ATTACK,     // 攻击
    DEFENSE,    // 防御
    HP          // 血量
} Capacity_List;

typedef struct  // 学生
{
    char name[21];          // 姓名
    int capacity[3];        // 能力。0=攻击 1=防御 2=血量
    float average;          // 平均值
} Student;  

typedef struct  // 学生名册
{
    Student students[100];  // 学生
    int count;              // 共有多少位学生
} Student_Book; 

void AddStudent(Student_Book *book);                // 添加学生
void PrintAllStudents(Student_Book *book);          // 打印所有学生
void FindStudent(Student_Book *book, char *name);   // 查找学生
void DeleteStudent(Student_Book *book, char *name); // 删除学生
void SortStudents(Student_Book *book);              // 按平均分从高到低排序学生
void PrintMenu(void);                               // 打印菜单

int main(void)
{
    Student_Book book = {0};
    char cmd;
    char name[21];

    PrintMenu();
    while ((cmd = getchar()) != EOF)
    {
        while (getchar() != '\n');

        switch (cmd)
        {
            case '1':   AddStudent(&book);
                break;
            case '2':   printf("输入要删除的学生姓名：");
                        scanf("%20s", name);
                        DeleteStudent(&book, name);
                break;
            case '3':   printf("输入要查找的学生姓名：");
                        scanf("%20s", name);
                        FindStudent(&book, name);
                break;
            case '4':   SortStudents(&book);
                break;
            case '5':   PrintAllStudents(&book);
                break;
            default : printf("输入错误\n");
        }
        PrintMenu();
    }

    return 0;
}

/**
 * @brief 添加学生
 * 
 * @param book Student_Book 结构体指针
 * @note 输入姓名和攻击、防御、血量，自动计算平均值
 */
void AddStudent(Student_Book *book)
{
    float average = 0;

    printf("\n输入姓名：");
    scanf("%20s", book->students[book->count].name);

    for (int i = 0; i < CAPACITY_LENGTH; i++)
    {
        printf("输入%s：", CapacityName[i]);
        scanf("%d", &book->students[book->count].capacity[i]);
        average += book->students[book->count].capacity[i];
    }

    book->students[book->count].average = average / (CAPACITY_LENGTH);
    printf("平均值是：%.2f", book->students[book->count].average);

    book->count++;
    printf("添加成功\n");
}

/**
 * @brief 打印所有学生
 * 
 * @param book Student_Book 结构体指针
 */
void PrintAllStudents(Student_Book *book)
{
    printf("\n正在打印所有学生......\n");
    for (int i = 0; i < book->count; i++)   // 遍历所有学生
    {
        printf("\n第 %d 位\n", i + 1);
        printf("姓名：%s\n", book->students[i].name);

        for (int j = 0; j < CAPACITY_LENGTH; j++)    // 遍历学生的能力
            printf("- %s：%d\n", CapacityName[j], book->students[i].capacity[j]);
        
        printf("- 平均值：%f\n", book->students[i].average);
    }
    printf("\n打印完成......\n");
}

/**
 * @brief 查找学生
 * 
 * @param book Student_Book 结构体指针
 * @param name 学生姓名
 */
void FindStudent(Student_Book *book, char *name)
{
    char flag = 0;

    printf("\n正在查找......\n");
    for (int i = 0; i < book->count; i++)
    {
        if (strcmp(book->students[i].name, name) == 0)
        {
            flag = 1;
            printf("找到了！\n");
            printf("姓名：%s\n", book->students[i].name);

            for (int j = 0; j < CAPACITY_LENGTH; j++)
            printf("- %s：%d\n", CapacityName[j], book->students[i].capacity[j]);
        
            printf("- 平均值：%f\n", book->students[i].average);

        }
    }
    if (flag == 0)
        printf("\n未找到......\n");
}

/**
 * @brief 删除学生
 * 
 * @param book Student_Book 结构体指针
 * @param name 学生姓名
 */
void DeleteStudent(Student_Book *book, char *name)
{
    char flag = 0;

    printf("\n正在查找......\n");
    for (int i = 0; i < book->count; i++)
    {
        if (strcmp(book->students[i].name, name) == 0)
        {
            flag = 1;
            printf("找到了！\n");

            for (int j  = i; j < book->count - 1; j++)  // 删除学生，book->students统一左移
                book->students[j] = book->students[j + 1];
            
            book->count--;
            printf("删除学生 %s 完成\n", name);
            break;
        }
    }
    if (flag == 0)
        printf("\n未找到......\n");
}

/**
 * @brief 按平均分从高到低排序学生
 * 
 * @param book Student_Book 结构体指针
 */
void SortStudents(Student_Book *book)
{
    printf("\n正在按平均分从高到低排序所有学生......\n");
    for (int i = 0; i < book->count - 1; i++)   // 遍历所有学生
    {
        Student temp;
        // 冒泡排序，从大到小排序
        for (int j = 0; j < book->count - 1 - i; j++)
        {
            if (book->students[j].average < book->students[j + 1].average)
            {
                temp = book->students[j + 1];
                book->students[j + 1] = book->students[j];
                book->students[j] = temp;
            }
        }
    }
    printf("\n排序完成......\n");
}

/**
 * @brief 打印菜单
 * 
 */
void PrintMenu(void)
{
    printf("\n---------------------------------------\n");
    printf("|||||||||||||学生管理系统||||||||||||||\n");
    printf("|-------------------------------------|\n");
    printf("|     添加学生     |     删除学生     |\n");
    printf("|        1         |        2         |\n");
    printf("|-------------------------------------|\n");
    printf("|     查找学生     | 按平均分排序学生 |\n");
    printf("|        3         |        4         |\n");
    printf("|-------------------------------------|\n");
    printf("|            打印所有学生             |\n");
    printf("|                 5                   |\n");
    printf("=======================================\n");
}