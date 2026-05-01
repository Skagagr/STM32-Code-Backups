#include <stdio.h>


/*
命令行通讯录
功能：
- 添加联系人（姓名、电话、备注）
- 删除联系人
- 查找联系人
- 显示所有联系人
- 退出程序后数据保存到文件，重新打开还在
*/

typedef struct
{
    char name[21];          // 名称
    char telephone[12];     // 电话号
    char note[51];          // 备注
} Contact;


typedef struct
{
    Contact contacts[100];  // 联系人的信息
    int count;              // 当前联系人
} Address_Book;


void AddContact(Address_Book *book);                        // 添加联系人
void PrintAllContacts(Address_Book *book);                  // 打印所有联系人
void FindContact(Address_Book *book, const char *name);     // 查找联系人
void DeleteContact(Address_Book *book, const char *name);   // 删除联系人
void PrintMenu(void);                                       // 打印菜单

int main(void)
{
    system("chcp 65001 > nul");  // 设置编码格式为 UTF-8

    Address_Book book = {0};
    char cmd;
    FILE *fp;

    // 每次运行程序就读取文件
    fp = fopen("Address_Book.bin", "rb");   // 读取方式打开文件（rb = 二进制读）
    if (fp != NULL)     // 文件存在才读取
    {
        fread(&book, sizeof(Address_Book), 1, fp);
        fclose(fp);
    }    

    PrintMenu();
    while ((cmd = getchar()) != EOF)
    {
        while (getchar() != '\n');

        if (cmd == '1')
            AddContact(&book);
        else if (cmd == '2')
        {
            char name[21];

            printf("输入要删除的联系人：");
            scanf("%20s", name);
            DeleteContact(&book, name);
        }
        else if (cmd == '3')
        {
            char name[21];

            printf("输入要查找的联系人：");
            scanf("%20s", name);
            FindContact(&book, name);
        }
        else if (cmd == '4')
            PrintAllContacts(&book);
        

        // 每次执行一次操作就写入文件
        fp = fopen("Address_Book.bin", "wb");       // 写入方式打开文件（wb = 二进制写）
        fwrite(&book, sizeof(Address_Book), 1, fp); // 一次性把整个结构体写进去
        fclose(fp);                                 // 关闭文件

        PrintMenu();
    }
    
    return 0;
}

/**
 * @brief 添加联系人
 * 
 * @param book Address_Book 结构体地址
 */
void AddContact(Address_Book *book)
{
    printf("输入名称：");
    scanf("%20s", book->contacts[book->count].name);

    printf("输入电话：");
    scanf("%11s", book->contacts[book->count].telephone);

    printf("输入简介：");
    scanf("%50s", book->contacts[book->count].note);


    printf("确认接收：\n");
    printf(" - 名称：%s\n", book->contacts[book->count].name);
    printf(" - 电话：%s\n", book->contacts[book->count].telephone);
    printf(" - 简介：%s\n", book->contacts[book->count].note);

    book->count++;
}

/**
 * @brief 打印所有联系人
 * 
 * @param book Address_Book 结构体地址
 */
void PrintAllContacts(Address_Book *book)
{
    if (book->count == 0)
        printf("\n当前无联系人\n\n");
    else
        printf("\n正在打印所有联系人\n");

    for (int i = 0; i < book->count; i++)
    {
        printf(" - 名称：%s\n", book->contacts[i].name);
        printf(" - 电话：%s\n", book->contacts[i].telephone);
        printf(" - 简介：%s\n", book->contacts[i].note);
        printf("\n");
    }
}

/**
 * @brief 查找联系人
 * 
 * @param book Address_Book 结构体地址
 * @param name 名称
 */
void FindContact(Address_Book *book, const char *name)
{
    int found = 0;

    for (int i = 0; i < book->count; i++)   // 遍历结构体内所有人员
    {
        if (strcmp(book->contacts[i].name, name) == 0)  // 查找指定名称的人
        {
            found = 1;

            printf("找到了，他的信息是：\n");
            printf(" - 名称：%s\n", book->contacts[i].name);
            printf(" - 电话：%s\n", book->contacts[i].telephone);
            printf(" - 简介：%s\n", book->contacts[i].note);

            break;
        }
    }

    if (found == 0)
        printf("没有找到哦\n");
}

/**
 * @brief 删除联系人
 * 
 * @param book Address_Book 结构体地址
 * @param name 名称
 */
void DeleteContact(Address_Book *book, const char *name)
{
    int found = 0;

    for (int i = 0; i < book->count; i++)
    {
        if (strcmp(book->contacts[i].name, name) == 0)
        {
            found = 1;

            // strcpy(book->contacts[i].name, book->contacts[i + 1].name);
            // strcpy(book->contacts[i].telephone, book->contacts[i + 1].telephone);
            // strcpy(book->contacts[i].note, book->contacts[i + 1].note);

            // 简化，结构体之间可以直接使用 '=' 赋值，将结构体内的数据统一进行了处理
            for (int j = i; j < book->count - 1; j ++)
                book->contacts[j] = book->contacts[j + 1];  
            break;
        }
    }
    if (found == 1)
    {
        book->count--;
        printf("删除完成\n");
    }
    else
        printf("没有找到哦\n");
    
}

/**
 * @brief 打印菜单
 * 
 */
void PrintMenu(void)
{
    printf("\t命令行通讯录\n");
    printf("-------------------------------\n");
    printf("| 添加联系人 | 删除联系人     |\n");
    printf("|     1      |     2          |\n");
    printf("|-----------------------------|\n");
    printf("| 查找联系人 | 打印所有联系人 |\n");
    printf("|     3      |     4          |\n");
    printf("-------------------------------\n");
}