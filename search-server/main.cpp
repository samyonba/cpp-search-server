// Решите загадку: Сколько чисел от 1 до 1000 содержат как минимум одну цифру 3?
// Напишите ответ здесь: 271

#include <iostream>

using namespace std;

int main()
{
    int count = 0;
    for (size_t i = 1; i < 1000; i++)
    {
        if (i % 10 == 3) {
            ++count;
            continue;
        }
        if (i/10 % 10 == 3) {
            ++count;
            continue;
        }
        if (i/100 % 10 == 3) {
            ++count;
            continue;
        }
    }
    cout << count << endl;

    // Ответ: 271
}
    
// Закомитьте изменения и отправьте их в свой репозиторий.
