#!/bin/bash
# Fix qsqlmon and PostgreSQL Qt framework paths to use bundled frameworks
# Run this after macdeployqt
# Usage: ./tools/macos-fix-qsqlmon.sh [bundle-path]

set -e

BUNDLE_PATH="${1:-install/quickevent.app}"
QSQLMON="$BUNDLE_PATH/Contents/MacOS/qsqlmon"
FRAMEWORKS="$BUNDLE_PATH/Contents/Frameworks"
SQLDRIVERS="$BUNDLE_PATH/Contents/PlugIns/sqldrivers"

if [ ! -f "$QSQLMON" ]; then
    echo "qsqlmon not found at $QSQLMON"
    exit 1
fi

echo "Fixing Qt framework paths in qsqlmon..."

for fw in QtSvg QtPrintSupport QtWidgets QtSql QtQml QtNetwork QtXml QtGui QtCore; do
    old_path=$(otool -L "$QSQLMON" | grep "/${fw}.framework" | awk '{print $1}')
    if [ -n "$old_path" ]; then
        install_name_tool -change "$old_path" "@executable_path/../Frameworks/${fw}.framework/Versions/A/${fw}" "$QSQLMON"
        echo "  Fixed $fw"
    fi
done

echo ""
echo "Fixing PostgreSQL (libpq) path in qsqlpsql plugin..."

PSQL_PLUGIN="$SQLDRIVERS/libqsqlpsql.dylib"

if [ ! -f "$PSQL_PLUGIN" ]; then
    echo "  WARNING: libqsqlpsql.dylib not found at $PSQL_PLUGIN, skipping."
else
    # Find the hardcoded libpq path — works for both universal and single arch binaries
    OLD_LIBPQ=$(otool -L "$PSQL_PLUGIN" | grep libpq | awk '{print $1}' | head -1)

    if [ -z "$OLD_LIBPQ" ]; then
        echo "  WARNING: No libpq dependency found in plugin, skipping."
    else
        echo "  Found hardcoded libpq path: $OLD_LIBPQ"

        # Prefer the versioned libpq.5.dylib to match what the plugin was built against
        BUNDLED_LIBPQ=$(find "$FRAMEWORKS" -name "libpq.5.dylib" | head -1)

        # Fall back to any libpq if versioned one is not found
        if [ -z "$BUNDLED_LIBPQ" ]; then
            BUNDLED_LIBPQ=$(find "$FRAMEWORKS" -name "libpq*.dylib" | head -1)
        fi

        if [ -z "$BUNDLED_LIBPQ" ]; then
            echo "  ERROR: No libpq found in $FRAMEWORKS"
            echo "  Make sure libpq is copied into the bundle before running this script."
            exit 1
        fi

        LIBPQ_NAME=$(basename "$BUNDLED_LIBPQ")
        echo "  Repointing to bundled: $LIBPQ_NAME"

        # Repoint the plugin — install_name_tool handles universal binaries automatically
        install_name_tool -change "$OLD_LIBPQ" \
            "@executable_path/../Frameworks/$LIBPQ_NAME" \
            "$PSQL_PLUGIN"

        # Fix libpq's own install name
        install_name_tool -id \
            "@executable_path/../Frameworks/$LIBPQ_NAME" \
            "$BUNDLED_LIBPQ"

        echo "  Fixed libpq -> $LIBPQ_NAME"

        # Fix any absolute paths inside libpq.5.dylib itself
        echo "  Checking libpq's own dependencies..."
        while IFS= read -r dep; do
            dep=$(echo "$dep" | awk '{print $1}')
            if [[ "$dep" == /opt/homebrew/* ]] || [[ "$dep" == /usr/local/* ]] || [[ "$dep" == /Applications/Postgres* ]]; then
                DEP_NAME=$(basename "$dep")
                echo "    Fixing transitive dep: $dep -> @executable_path/../Frameworks/$DEP_NAME"
                install_name_tool -change "$dep" \
                    "@executable_path/../Frameworks/$DEP_NAME" \
                    "$BUNDLED_LIBPQ"
            fi
        done < <(otool -L "$BUNDLED_LIBPQ" | tail -n +2)
    fi
fi

echo ""
echo "Fixing ODBC (libiodbc) path in qsqlodbc plugin..."

ODBC_PLUGIN="$SQLDRIVERS/libqsqlodbc.dylib"

if [ ! -f "$ODBC_PLUGIN" ]; then
    echo "  WARNING: libqsqlodbc.dylib not found at $SQLDRIVERS, skipping."
else
    OLD_IODBC=$(otool -L "$ODBC_PLUGIN" | grep libiodbc | awk '{print $1}' | head -1)

    if [ -z "$OLD_IODBC" ]; then
        echo "  WARNING: No libiodbc dependency found in plugin, skipping."
    else
        echo "  Found hardcoded libiodbc path: $OLD_IODBC"

        BUNDLED_IODBC=$(find "$FRAMEWORKS" -name "libiodbc*.dylib" | head -1)

        if [ -z "$BUNDLED_IODBC" ]; then
            echo "  WARNING: libiodbc not found in bundle Frameworks, skipping."
        else
            IODBC_NAME=$(basename "$BUNDLED_IODBC")
            install_name_tool -change "$OLD_IODBC" \
                "@executable_path/../Frameworks/$IODBC_NAME" \
                "$ODBC_PLUGIN"
            install_name_tool -id \
                "@executable_path/../Frameworks/$IODBC_NAME" \
                "$BUNDLED_IODBC"
            echo "  Fixed libiodbc -> $IODBC_NAME"
        fi
    fi
fi