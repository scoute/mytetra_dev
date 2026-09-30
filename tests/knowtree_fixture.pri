# Фикстура минимальной базы знаний для тестов третьей очереди.
# Копирует tests/fixtures/knowtree/data (mytetra.xml + тексты записей)
# в каталог сборки теста. Оттуда модель читает базу через tetradir=./data.
# Копия в сборке изолирует прогон: save() и database.ini правят только ее,
# эталон в репозитории и данные пользователя не трогаются.

# $$PWD здесь - каталог самого .pri (tests/), поэтому путь без подъема.
FIXTURE_SRC = $$PWD/fixtures/knowtree/data
!exists($$FIXTURE_SRC/mytetra.xml) {
    error("Нет фикстуры базы $$FIXTURE_SRC/mytetra.xml")
}
QMAKE_POST_LINK += $(COPY_DIR) $$quote($$FIXTURE_SRC) $$quote($$OUT_PWD/data) $$escape_expand(\n\t)
