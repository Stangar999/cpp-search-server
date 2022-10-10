# cpp-search-server
## Поисковая система

## Цель проекта
Обучение.

## Описание проекта
Поисковая система, принимает на вход документы, добавляет их в базу документов. По желанию задаются стоп-слова, которые не будут участвовать в поиске(предлоги например).
При получении поискового запроса система ещет пододящие документы и выдает результаты по релевантности. Также при поиске есть возможность учитывать статус и рейтинг
документов. 

## Cистемные требования
- С++17;

## Инструкция по развертыванию проекта
Просто собрать и запустить
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
