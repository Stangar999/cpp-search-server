# SearchServer
## Поисковая система  
Программная реализация поисковой системы. Программа принимает на вход документы, добавляет их в базу документов. По желанию задаются стоп-слова, которые не будут учитываться при поиске (предлоги например).
При получении поискового запроса система ищет подходящие документы и выдает результаты по релевантности. Также при поиске есть возможность учитывать статус и рейтинг
документов. 

## Использование
0. Установка и настройка всех требуемых компонентов в среде разработки для запуска приложения
1. Вариант использования показан ниже
<details><summary>Пример как создать заполнить и выполнить поиск документа</summary>
    
~~~
int main() {
    // Создать поисковую систему в конструктор можно передать стоп слова
    SearchServer search_server("and in at"s);
    // Наполнить ее документами. Возможные параметры документа: id, сам документ, статус документа, рейтинг 
    search_server.AddDocument(1, "curly cat curly tail"s, DocumentStatus::ACTUAL, {7, 2, 7});
    search_server.AddDocument(2, "curly dog and fancy collar"s, DocumentStatus::ACTUAL, {1, 2, 3});
    search_server.AddDocument(3, "big cat fancy collar "s, DocumentStatus::ACTUAL, {1, 2, 8});
    search_server.AddDocument(4, "big dog sparrow Eugene"s, DocumentStatus::ACTUAL, {1, 3, 2});
    search_server.AddDocument(5, "big dog sparrow Vasiliy"s, DocumentStatus::ACTUAL, {1, 1, 1});
    // Запрос на поис в документах, результат в std::cout
    FindTopDocuments(search_server, "big dog"s);
    return 0;
}
~~~
</details>

## Cистемные требования:
- С++17 (STL);
- GCC (MinGW-w64) 11.2.0.
