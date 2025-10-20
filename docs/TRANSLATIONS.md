# Multi-Language Support

This document explains how to use and extend the translation system in Standard of Iron.

## Supported Languages

Currently, the application supports:
- **English (en)** - Default language
- **German (de)** - Full translation

## Build-Time Language Selection

You can set the default language at build time using the `DEFAULT_LANG` variable:

```bash
# Build with English (default)
make build

# Build with German
DEFAULT_LANG=de make build
```

Or with CMake directly:

```bash
cmake -DDEFAULT_LANG=de ..
make
```

## Runtime Language Switching

Users can change the language at runtime through the Settings panel:

1. Launch the application
2. Navigate to **Settings** from the main menu
3. Select the desired language from the dropdown
4. Changes apply immediately to all UI elements

## Translation Files

Translation files are located in the `translations/` directory:

- `app_en.ts` - English source translation file (XML format)
- `app_de.ts` - German source translation file (XML format)
- `app_en.qm` - English compiled translation file (binary)
- `app_de.qm` - German compiled translation file (binary)

## Adding a New Language

To add support for a new language (e.g., French):

### 1. Create Translation Source File

```bash
cd translations
cp app_en.ts app_fr.ts
```

### 2. Update Language Code

Edit `app_fr.ts` and change the language attribute:

```xml
<TS version="2.1" language="fr_FR">
```

### 3. Translate Strings

Option A: Use Qt Linguist (GUI tool):

```bash
linguist translations/app_fr.ts
```

Option B: Edit the XML file manually:

```xml
<message>
    <source>Play</source>
    <translation>Jouer</translation>
</message>
```

### 4. Generate Binary Translation File

```bash
lrelease translations/app_fr.ts -qm translations/app_fr.qm
```

### 5. Update Build System

Edit `CMakeLists.txt` and add the new language to the RESOURCES section:

```cmake
RESOURCES
    ...
    translations/app_en.qm
    translations/app_de.qm
    translations/app_fr.qm  # Add this line
```

For Qt5 builds, also update the `translations.qrc` file:

```xml
<RCC>
    <qresource prefix="/translations">
        <file>translations/app_en.qm</file>
        <file>translations/app_de.qm</file>
        <file>translations/app_fr.qm</file>  <!-- Add this -->
    </qresource>
</RCC>
```

### 6. Update LanguageManager

Edit `app/core/language_manager.cpp` to include the new language:

```cpp
LanguageManager::LanguageManager(QObject *parent)
    : QObject(parent), m_currentLanguage("en"), m_translator(new QTranslator(this)) {
  m_availableLanguages << "en"
                       << "de"
                       << "fr";  // Add this line
  // ...
}

QString LanguageManager::languageDisplayName(const QString &language) const {
  if (language == "en") {
    return "English";
  } else if (language == "de") {
    return "Deutsch (German)";
  } else if (language == "fr") {  // Add this
    return "Français (French)";
  }
  return language;
}
```

### 7. Rebuild

```bash
make rebuild
```

## Updating Existing Translations

When you add new UI strings or modify existing ones:

### 1. Update Translation Source Files

Use `lupdate` to scan QML files and update translation files:

```bash
lupdate ui/qml/*.qml -ts translations/app_en.ts translations/app_de.ts
```

This will:
- Add new translatable strings
- Mark removed strings as obsolete
- Preserve existing translations

### 2. Translate New Strings

Use Qt Linguist or edit the `.ts` files to translate new strings.

### 3. Regenerate Binary Files

```bash
lrelease translations/app_en.ts -qm translations/app_en.qm
lrelease translations/app_de.ts -qm translations/app_de.qm
```

### 4. Rebuild the Application

```bash
make rebuild
```

## Translation Best Practices

1. **Use `qsTr()` in QML**: Wrap all user-visible strings with `qsTr()`:
   ```qml
   Text { text: qsTr("Hello, World!") }
   ```

2. **Preserve Placeholders**: Keep `%1`, `%2` etc. in translations:
   ```xml
   <source>Units: %1 / %2</source>
   <translation>Einheiten: %1 / %2</translation>
   ```

3. **Context Matters**: The same English word may need different translations in different contexts. Qt Linguist shows the context (QML file and line number).

4. **Test All Languages**: After adding translations, test the application in each language to ensure:
   - Text fits in UI elements
   - Formatting is correct
   - Meaning is preserved

## Architecture

### LanguageManager Class

Located in `app/core/language_manager.{h,cpp}`, this class:
- Manages available languages
- Loads `.qm` translation files at runtime
- Notifies the QML UI when language changes
- Provides display names for languages

### QML Integration

The `LanguageManager` is exposed to QML as a context property:

```cpp
engine.rootContext()->setContextProperty("languageManager", languageManager);
```

QML components can then:
- Access current language: `languageManager.currentLanguage`
- Get available languages: `languageManager.availableLanguages`
- Change language: `languageManager.setLanguage("de")`
- Get display name: `languageManager.languageDisplayName("de")`

### Resource System

Translation files are embedded in the application binary using Qt's resource system:
- Qt6: Files listed in `CMakeLists.txt` RESOURCES section
- Qt5: Files listed in `translations.qrc`

Files are accessed at runtime with the prefix: `:/StandardOfIron/translations/app_XX.qm`

## Troubleshooting

### Translation not loading

Check that:
1. The `.qm` file exists in `translations/`
2. The file is listed in `CMakeLists.txt` (Qt6) or `translations.qrc` (Qt5)
3. The application was rebuilt after adding the file
4. Check console output for "Language changed to:" message

### Strings not translating

Ensure:
1. Strings are wrapped with `qsTr()` in QML
2. Translation exists in the `.ts` file
3. Translation is not marked as "unfinished"
4. `.qm` file was regenerated after updating `.ts`
5. Application was rebuilt

### UI layout issues

If translated text doesn't fit:
1. Use shorter translations where possible
2. Adjust QML layouts to accommodate longer text
3. Use text eliding: `elide: Text.ElideRight`
4. Test with German (often longer than English)

## Tools

- **lupdate**: Scans source code for translatable strings, updates `.ts` files
- **lrelease**: Compiles `.ts` files to binary `.qm` files
- **Qt Linguist**: GUI tool for managing translations (optional)

All tools are included with Qt development packages.
