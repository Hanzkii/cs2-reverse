// Source 2 C-Parser & SDK Type Importer for Ghidra
// Headlessly populates all Source 2 SchemaSystem and C++20 reconstructed entity classes into Ghidra's DataTypeManager.
//@category Source2
//@description Imports Source 2 Schema & C++20 SDK Types into /Source2/Classes and /Source2/BaseTypes.
//@menupath Tools.Source2.Import SDK Types

import ghidra.app.script.GhidraScript;
import ghidra.program.model.data.*;

import java.io.File;
import java.nio.file.Files;
import java.util.*;

public class Source2_ImportSDKTypes extends GhidraScript {

    @Override
    public void run() throws Exception {
        if (currentProgram == null) {
            printerr("[-] ERROR: No open program in Ghidra context.");
            return;
        }

        String rawProgName = currentProgram.getName();
        String progName = normalizeProgramName(rawProgName);
        println("[*] Active Ghidra Program: " + rawProgName + " (normalized: " + progName + ")");

        File typesFile = resolveTypesFile();
        if (typesFile == null || !typesFile.exists()) {
            printerr("[-] ERROR: Could not locate SDK types file (source2_types.h or ghidra_sdk_classes.json).");
            return;
        }
        println("[*] Loading SDK types from: " + typesFile.getAbsolutePath());

        DataTypeManager dtm = currentProgram.getDataTypeManager();
        int transactionID = dtm.startTransaction("Import Source 2 SDK Types");
        boolean success = false;

        try {
            // 1. Ensure /Source2/BaseTypes Category and foundational primitives
            ensureBaseTypes(dtm);

            CategoryPath baseCat = new CategoryPath("/Source2/BaseTypes");
            CategoryPath classCat = new CategoryPath("/Source2/Classes");

            DataType typeVec = dtm.getDataType(baseCat, "Vector");
            DataType typeVec2 = dtm.getDataType(baseCat, "Vector2D");
            DataType typeVec4 = dtm.getDataType(baseCat, "Vector4D");
            DataType typeQAngle = dtm.getDataType(baseCat, "QAngle");
            DataType typeHandle = dtm.getDataType(baseCat, "CEntityHandle");
            DataType typeQuat = dtm.getDataType(baseCat, "Quaternion");
            DataType typeTransform = dtm.getDataType(baseCat, "CTransform");
            DataType typeUtlStr = dtm.getDataType(baseCat, "CUtlString");
            DataType typeUtlSym = dtm.getDataType(baseCat, "CUtlSymbolLarge");
            DataType typeUtlTok = dtm.getDataType(baseCat, "CUtlStringToken");
            DataType typeUtlVec = dtm.getDataType(baseCat, "CUtlVector_void");

            // 2. Parse and import classes into /Source2/Classes
            int createdCount = 0;
            int fieldsCount = 0;

            String fileName = typesFile.getName().toLowerCase();
            if (fileName.endsWith(".json")) {
                String jsonContent = new String(Files.readAllBytes(typesFile.toPath()), "UTF-8");
                Map<String, Object> rootJson = parseJson(jsonContent);
                if (rootJson == null) {
                    printerr("[-] ERROR: Failed to parse SDK types JSON.");
                    return;
                }

                @SuppressWarnings("unchecked")
                Map<String, Object> classesMap = (Map<String, Object>) rootJson.get("classes");
                if (classesMap == null) {
                    classesMap = rootJson;
                }

                println("[*] Ingesting " + classesMap.size() + " classes into " + classCat.getPath() + "...");

                for (Map.Entry<String, Object> entry : classesMap.entrySet()) {
                    String className = entry.getKey();
                    @SuppressWarnings("unchecked")
                    Map<String, Object> classData = (Map<String, Object>) entry.getValue();

                    String binary = (String) classData.get("binary");
                    if (binary != null && !binary.isEmpty()) {
                        String cleanBin = binary.toLowerCase();
                        if (!progName.contains(cleanBin) && !cleanBin.contains(progName) && !progName.equals("all")) {
                            continue;
                        }
                    }

                    Number sizeNum = (Number) classData.get("size");
                    int classSize = (sizeNum != null) ? sizeNum.intValue() : 0;
                    if (classSize <= 0) classSize = 8;

                    String baseClass = (String) classData.get("base");

                    StructureDataType struct = new StructureDataType(classCat, className, classSize, dtm);
                    if (baseClass != null && !baseClass.isEmpty() && !baseClass.equalsIgnoreCase("None")) {
                        struct.setDescription("Base: " + baseClass + " | Size: 0x" + Integer.toHexString(classSize) + " (" + classSize + "b)");
                    } else {
                        struct.setDescription("Size: 0x" + Integer.toHexString(classSize) + " (" + classSize + "b)");
                    }

                    @SuppressWarnings("unchecked")
                    List<Map<String, Object>> fields = (List<Map<String, Object>>) classData.get("fields");
                    if (fields != null && !fields.isEmpty()) {
                        List<Map<String, Object>> sortedFields = new ArrayList<>(fields);
                        sortedFields.sort(Comparator.comparingInt(f -> ((Number) f.get("offset")).intValue()));

                        for (int i = 0; i < sortedFields.size(); i++) {
                            Map<String, Object> f = sortedFields.get(i);
                            String fieldName = (String) f.get("name");
                            String fieldTypeStr = (String) f.get("type");
                            int offset = ((Number) f.get("offset")).intValue();

                            if (offset < 0 || offset >= classSize) continue;

                            int span = 4;
                            if (i + 1 < sortedFields.size()) {
                                int nextOff = ((Number) sortedFields.get(i + 1).get("offset")).intValue();
                                if (nextOff > offset) span = nextOff - offset;
                            } else {
                                span = classSize - offset;
                            }

                            DataType fType = resolveFieldType(
                                fieldName, fieldTypeStr, span, dtm, classCat,
                                typeVec, typeVec2, typeVec4, typeQAngle,
                                typeHandle, typeQuat, typeTransform,
                                typeUtlStr, typeUtlSym, typeUtlTok, typeUtlVec
                            );

                            int fSize = fType.getLength();
                            if (fSize <= 0) fSize = span;

                            try {
                                if (offset + fSize <= struct.getLength()) {
                                    struct.replaceAtOffset(offset, fType, fSize, fieldName, "Offset: +0x" + Integer.toHexString(offset));
                                    fieldsCount++;
                                }
                            } catch (Exception ignored) {
                            }
                        }
                    }

                    dtm.addDataType(struct, DataTypeConflictHandler.REPLACE_HANDLER);
                    createdCount++;
                }

            } else {
                // Parse directly from C Header data/source2_types.h
                println("[*] Parsing C header: " + typesFile.getName() + "...");
                String content = new String(Files.readAllBytes(typesFile.toPath()), "UTF-8");
                createdCount = parseAndImportCHeader(content, progName, dtm, classCat, baseCat);
            }

            success = true;
            println("=================================================================");
            println("[+] SUCCESS: Created " + createdCount + " structures with " + fieldsCount + " typed fields");
            println("[+] Category: " + classCat.getPath());
            println("=================================================================");

        } finally {
            dtm.endTransaction(transactionID, success);
        }
    }

    private void ensureBaseTypes(DataTypeManager dtm) {
        CategoryPath cat = new CategoryPath("/Source2/BaseTypes");

        // Vector (12 bytes)
        StructureDataType vec = new StructureDataType(cat, "Vector", 12, dtm);
        vec.replaceAtOffset(0, FloatDataType.dataType, 4, "x", "X coordinate");
        vec.replaceAtOffset(4, FloatDataType.dataType, 4, "y", "Y coordinate");
        vec.replaceAtOffset(8, FloatDataType.dataType, 4, "z", "Z coordinate");
        dtm.addDataType(vec, DataTypeConflictHandler.REPLACE_HANDLER);

        // Vector2D (8 bytes)
        StructureDataType vec2 = new StructureDataType(cat, "Vector2D", 8, dtm);
        vec2.replaceAtOffset(0, FloatDataType.dataType, 4, "x", "X coordinate");
        vec2.replaceAtOffset(4, FloatDataType.dataType, 4, "y", "Y coordinate");
        dtm.addDataType(vec2, DataTypeConflictHandler.REPLACE_HANDLER);

        // Vector4D (16 bytes)
        StructureDataType vec4 = new StructureDataType(cat, "Vector4D", 16, dtm);
        vec4.replaceAtOffset(0, FloatDataType.dataType, 4, "x", "X coordinate");
        vec4.replaceAtOffset(4, FloatDataType.dataType, 4, "y", "Y coordinate");
        vec4.replaceAtOffset(8, FloatDataType.dataType, 4, "z", "Z coordinate");
        vec4.replaceAtOffset(12, FloatDataType.dataType, 4, "w", "W coordinate");
        dtm.addDataType(vec4, DataTypeConflictHandler.REPLACE_HANDLER);

        // QAngle (12 bytes)
        StructureDataType qangle = new StructureDataType(cat, "QAngle", 12, dtm);
        qangle.replaceAtOffset(0, FloatDataType.dataType, 4, "pitch", "Pitch (-89 to 89 deg)");
        qangle.replaceAtOffset(4, FloatDataType.dataType, 4, "yaw", "Yaw (-180 to 180 deg)");
        qangle.replaceAtOffset(8, FloatDataType.dataType, 4, "roll", "Roll bank angle");
        dtm.addDataType(qangle, DataTypeConflictHandler.REPLACE_HANDLER);

        // Quaternion (16 bytes)
        StructureDataType quat = new StructureDataType(cat, "Quaternion", 16, dtm);
        quat.replaceAtOffset(0, FloatDataType.dataType, 4, "x", "X component");
        quat.replaceAtOffset(4, FloatDataType.dataType, 4, "y", "Y component");
        quat.replaceAtOffset(8, FloatDataType.dataType, 4, "z", "Z component");
        quat.replaceAtOffset(12, FloatDataType.dataType, 4, "w", "W component");
        dtm.addDataType(quat, DataTypeConflictHandler.REPLACE_HANDLER);

        // CTransform (32 bytes aligned)
        StructureDataType transform = new StructureDataType(cat, "CTransform", 32, dtm);
        transform.replaceAtOffset(0, quat, 16, "rot", "Rodrigues rotation quaternion");
        transform.replaceAtOffset(16, vec, 12, "pos", "Translation world position");
        transform.replaceAtOffset(28, FloatDataType.dataType, 4, "pad", "Alignment pad");
        dtm.addDataType(transform, DataTypeConflictHandler.REPLACE_HANDLER);

        // CEntityHandle (4 bytes bitfield: index & 0x7FFF, serial >> 15)
        StructureDataType handle = new StructureDataType(cat, "CEntityHandle", 4, dtm);
        handle.replaceAtOffset(0, UnsignedIntegerDataType.dataType, 4, "m_Index", "Bits 0-14: Index | Bits 15-31: Serial");
        dtm.addDataType(handle, DataTypeConflictHandler.REPLACE_HANDLER);

        // CUtlString (8 bytes pointer)
        StructureDataType utlStr = new StructureDataType(cat, "CUtlString", 8, dtm);
        utlStr.replaceAtOffset(0, PointerDataType.dataType, 8, "m_pszString", "Char buffer pointer");
        dtm.addDataType(utlStr, DataTypeConflictHandler.REPLACE_HANDLER);

        // CUtlSymbolLarge (8 bytes pointer)
        StructureDataType utlSym = new StructureDataType(cat, "CUtlSymbolLarge", 8, dtm);
        utlSym.replaceAtOffset(0, PointerDataType.dataType, 8, "m_pString", "Interned string table pointer");
        dtm.addDataType(utlSym, DataTypeConflictHandler.REPLACE_HANDLER);

        // CUtlStringToken (4 bytes MurmurHash2 token)
        StructureDataType utlTok = new StructureDataType(cat, "CUtlStringToken", 4, dtm);
        utlTok.replaceAtOffset(0, UnsignedIntegerDataType.dataType, 4, "m_nHashCode", "MurmurHash2 token");
        dtm.addDataType(utlTok, DataTypeConflictHandler.REPLACE_HANDLER);

        // CUtlVector_void (16 bytes)
        StructureDataType utlVec = new StructureDataType(cat, "CUtlVector_void", 16, dtm);
        utlVec.replaceAtOffset(0, IntegerDataType.dataType, 4, "m_Size", "Current element count");
        utlVec.replaceAtOffset(4, IntegerDataType.dataType, 4, "m_Capacity", "Allocated capacity");
        utlVec.replaceAtOffset(8, PointerDataType.dataType, 8, "m_pElements", "Elements pointer");
        dtm.addDataType(utlVec, DataTypeConflictHandler.REPLACE_HANDLER);
    }

    private DataType resolveFieldType(
        String fieldName, String explicitType, int span, DataTypeManager dtm, CategoryPath classCat,
        DataType vec, DataType vec2, DataType vec4, DataType qang,
        DataType handle, DataType quat, DataType transform,
        DataType utlStr, DataType utlSym, DataType utlTok, DataType utlVec
    ) {
        if (explicitType != null && !explicitType.isEmpty()) {
            String t = explicitType.trim();
            if (t.endsWith("*")) return PointerDataType.dataType;
            if (t.equals("Vector") || t.equals("Vector3")) return vec;
            if (t.equals("Vector2D")) return vec2;
            if (t.equals("Vector4D")) return vec4;
            if (t.equals("QAngle")) return qang;
            if (t.equals("Quaternion")) return quat;
            if (t.equals("CTransform")) return transform;
            if (t.equals("CEntityHandle")) return handle;
            if (t.equals("CUtlString")) return utlStr;
            if (t.equals("CUtlSymbolLarge")) return utlSym;
            if (t.equals("CUtlStringToken")) return utlTok;
            if (t.equals("CUtlVector_void")) return utlVec;
            if (t.equals("bool")) return BooleanDataType.dataType;
            if (t.equals("float")) return FloatDataType.dataType;
            if (t.equals("double")) return DoubleDataType.dataType;
            if (t.equals("int8_t")) return SignedByteDataType.dataType;
            if (t.equals("uint8_t")) return ByteDataType.dataType;
            if (t.equals("int16_t") || t.equals("short")) return ShortDataType.dataType;
            if (t.equals("uint16_t")) return UnsignedShortDataType.dataType;
            if (t.equals("int32_t") || t.equals("int")) return IntegerDataType.dataType;
            if (t.equals("uint32_t")) return UnsignedIntegerDataType.dataType;
            if (t.equals("int64_t")) return LongLongDataType.dataType;
            if (t.equals("uint64_t") || t.equals("uintptr_t")) return UnsignedLongLongDataType.dataType;
        }

        // Deduce based on Hungarian naming conventions
        if (fieldName != null) {
            if (fieldName.startsWith("m_p") || (fieldName.startsWith("p") && fieldName.length() > 2 && Character.isUpperCase(fieldName.charAt(1)))) {
                return PointerDataType.dataType;
            }
            if (fieldName.startsWith("m_b") || (fieldName.startsWith("b") && fieldName.length() > 2 && Character.isUpperCase(fieldName.charAt(1)))) {
                return BooleanDataType.dataType;
            }
            if (fieldName.startsWith("m_fl") || (fieldName.startsWith("fl") && fieldName.length() > 3 && Character.isUpperCase(fieldName.charAt(2)))) {
                return FloatDataType.dataType;
            }
            if (fieldName.startsWith("m_h") && handle != null) {
                return handle;
            }
            if (fieldName.startsWith("m_vec")) {
                if (span == 8 && vec2 != null) return vec2;
                if (span == 16 && vec4 != null) return vec4;
                if (vec != null) return vec;
            }
            if (fieldName.startsWith("m_ang") && qang != null) {
                return qang;
            }
            if (fieldName.startsWith("m_quat") && quat != null) {
                return quat;
            }
        }

        // Fallback based on byte span
        if (span == 1) return ByteDataType.dataType;
        if (span == 2) return ShortDataType.dataType;
        if (span == 4) return IntegerDataType.dataType;
        if (span == 8) return LongLongDataType.dataType;
        if (span == 12 && vec != null) return vec;
        if (span == 16 && vec4 != null) return vec4;

        return Undefined4DataType.dataType;
    }

    private int parseAndImportCHeader(String content, String progName, DataTypeManager dtm, CategoryPath classCat, CategoryPath baseCat) {
        // Fallback regex parser for source2_types.h directly inside Ghidra
        java.util.regex.Pattern structPat = java.util.regex.Pattern.compile(
            "/\\*\\s*Binary:\\s*([^|]*?)\\s*\\|\\s*Base:\\s*([^|]*?)\\s*\\|\\s*Size:\\s*(\\d+)\\s*\\([^)]+\\)\\s*\\*/\\s*struct\\s+([A-Za-z0-9_]+)\\s*\\{([^}]+)\\};",
            java.util.regex.Pattern.DOTALL
        );
        java.util.regex.Matcher m = structPat.matcher(content);
        int count = 0;

        while (m.find()) {
            String binary = m.group(1).trim().toLowerCase();
            String base = m.group(2).trim();
            int size = Integer.parseInt(m.group(3).trim());
            String name = m.group(4).trim();

            if (!binary.isEmpty() && !progName.contains(binary) && !binary.contains(progName) && !progName.equals("all")) {
                continue;
            }

            StructureDataType struct = new StructureDataType(classCat, name, size, dtm);
            if (!base.isEmpty() && !base.equalsIgnoreCase("None")) {
                struct.setDescription("Base: " + base + " | Size: 0x" + Integer.toHexString(size));
            } else {
                struct.setDescription("Size: 0x" + Integer.toHexString(size));
            }
            dtm.addDataType(struct, DataTypeConflictHandler.REPLACE_HANDLER);
            count++;
        }
        return count;
    }

    private String normalizeProgramName(String raw) {
        String s = raw.toLowerCase();
        if (s.contains("/")) s = s.substring(s.lastIndexOf('/') + 1);
        if (s.contains("\\")) s = s.substring(s.lastIndexOf('\\') + 1);
        if (s.contains(".")) {
            String[] parts = s.split("\\.");
            if (parts.length > 2 && parts[parts.length - 1].matches("\\d+")) {
                s = parts[0] + "." + parts[1];
            }
        }
        return s;
    }

    private File resolveTypesFile() {
        String[] args = getScriptArgs();
        if (args != null && args.length > 0 && !args[0].isEmpty()) {
            File f = new File(args[0]);
            if (f.exists()) return f;
        }

        String[] candidates = {
            "data/ghidra_sdk_classes.json",
            "data/source2_types.h",
            "../data/ghidra_sdk_classes.json",
            "../data/source2_types.h",
            "../../data/ghidra_sdk_classes.json",
            "../../data/source2_types.h"
        };
        for (String c : candidates) {
            File cand = new File(c);
            if (cand.exists()) return cand;
        }
        return null;
    }

    @SuppressWarnings("unchecked")
    private Map<String, Object> parseJson(String json) {
        try {
            Class<?> gsonClass = Class.forName("com.google.gson.Gson");
            Object gson = gsonClass.getDeclaredConstructor().newInstance();
            java.lang.reflect.Method fromJson = gsonClass.getMethod("fromJson", String.class, Class.class);
            return (Map<String, Object>) fromJson.invoke(gson, json, Map.class);
        } catch (Exception e) {
            printerr("Gson parse error: " + e.getMessage());
            return null;
        }
    }
}
