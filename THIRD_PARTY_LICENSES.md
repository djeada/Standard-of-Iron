# Third-Party Software Licenses

This document lists the licenses of third-party software used in Standard of Iron.

## Qt Framework

**License:** GNU Lesser General Public License v3 (LGPL v3)  
**Website:** https://www.qt.io  
**License Text:** https://www.gnu.org/licenses/lgpl-3.0.html  
**Source Code:** https://www.qt.io/download-open-source

### LGPL v3 Compliance

Standard of Iron complies with the LGPL v3 requirements for using Qt:

1. **Dynamic Linking**: Qt is dynamically linked to the application, not statically linked.
   - On Windows: Qt DLLs are deployed alongside the executable using `windeployqt`
   - On Linux: Qt shared libraries (.so) are linked dynamically
   - On macOS: Qt frameworks are linked dynamically

2. **No Modifications**: We do not modify Qt source code. If modifications were made, they would be released under LGPL v3.

3. **License Notice**: LGPL v3 attribution is provided in:
   - This document (THIRD_PARTY_LICENSES.md)
   - README.md (License section)
   - In-game Settings panel (About section)

4. **User Re-linking**: Users can replace the Qt libraries with their own versions because Qt is dynamically linked. This is automatic with dynamic linking - users simply replace the Qt DLLs/shared libraries in the application directory.

### Qt Components Used

- QtCore
- QtGui
- QtWidgets
- QtOpenGL
- QtQuick
- QtQml
- QtQuickControls2
- QtSql
- QtMultimedia

### Verification

To verify dynamic linking:

**Windows:**
```powershell
dumpbin /DEPENDENTS standard_of_iron.exe | findstr Qt
```

**Linux:**
```bash
ldd standard_of_iron | grep Qt
```

**macOS:**
```bash
otool -L standard_of_iron | grep Qt
```

All commands should show Qt libraries as external dependencies, confirming dynamic linking.
