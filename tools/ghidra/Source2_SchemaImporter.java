// Source 2 Schema to Ghidra DataTypeManager Importer
// Parses Source 2 schemas.json and populates Ghidra structures with exact field offsets and types.
//@category Source2
//@description Imports Source 2 Schema classes and fields into Ghidra's DataTypeManager.
//@menupath Tools.Source2.Import Schema Types

import ghidra.app.script.GhidraScript;
import ghidra.program.model.data.*;
import java.io.File;
import java.nio.file.Files;
import java.util.*;

public class Source2_SchemaImporter extends GhidraScript {

    @Override
    public void run() throws Exception {
        if (currentProgram == null) {
            printerr("[-] ERROR: No open program in Ghidra.");
            return;
        }

        String progName = currentProgram.getName().toLowerCase();
        println("[*] Target Program: " + progName);

        // Resolve schemas.json location
        File schemasFile = resolveSchemasFile();
        if (schemasFile == null || !schemasFile.exists()) {
            printerr("[-] ERROR: Could not locate schemas.json.");
            return;
        }
        println("[*] Loading schemas from: " + schemasFile.getAbsolutePath());

        // Parse JSON using Gson
        String jsonContent = new String(Files.readAllBytes(schemasFile.toPath()), "UTF-8");
        Map<String, Object> schemaMap = parseJson(jsonContent);
        if (schemaMap == null) {
            printerr("[-] ERROR: Failed to parse schemas.json.");
            return;
        }
        println("[*] Total schema definitions parsed: " + schemaMap.size());

        DataTypeManager dtm = currentProgram.getDataTypeManager();
        int transactionID = dtm.startTransaction("Import Source 2 Schemas");
        boolean success = false;

        try {
            // First ensure base types exist
            ensureBaseTypes(dtm);

            CategoryPath progCat = new CategoryPath("/Source2/" + currentProgram.getName());
            CategoryPath baseCat = new CategoryPath("/Source2/BaseTypes");

            DataType typeVec = dtm.getDataType(baseCat, "Vector");
            DataType typeVec2 = dtm.getDataType(baseCat, "Vector2D");
            DataType typeVec4 = dtm.getDataType(baseCat, "Vector4D");
            DataType typeQAngle = dtm.getDataType(baseCat, "QAngle");
            DataType typeHandle = dtm.getDataType(baseCat, "CEntityHandle");
            DataType typeQuat = dtm.getDataType(baseCat, "Quaternion");
            DataType typeUtlStr = dtm.getDataType(baseCat, "CUtlString");

            int createdCount = 0;
            int fieldsCount = 0;

            for (Map.Entry<String, Object> entry : schemaMap.entrySet()) {
                String className = entry.getKey();
                @SuppressWarnings("unchecked")
                Map<String, Object> classData = (Map<String, Object>) entry.getValue();

                String binary = (String) classData.get("binary");
                // Import if binary matches current program or if program contains module name
                if (binary != null && !binary.isEmpty()) {
                    String cleanBin = binary.toLowerCase();
                    if (!progName.contains(cleanBin) && !cleanBin.contains(progName)) {
                        continue;
                    }
                }

                Number sizeNum = (Number) classData.get("size");
                int classSize = (sizeNum != null) ? sizeNum.intValue() : 0;
                if (classSize <= 0) classSize = 8;

                String baseClass = (String) classData.get("base");

                StructureDataType struct = new StructureDataType(progCat, className, classSize, dtm);
                if (baseClass != null && !baseClass.isEmpty()) {
                    struct.setDescription("Base: " + baseClass + " | Size: " + classSize);
                } else {
                    struct.setDescription("Size: " + classSize);
                }

                @SuppressWarnings("unchecked")
                List<Map<String, Object>> fields = (List<Map<String, Object>>) classData.get("fields");
                if (fields != null && !fields.isEmpty()) {
                    // Sort fields by offset
                    List<Map<String, Object>> sortedFields = new ArrayList<>(fields);
                    sortedFields.sort(Comparator.comparingInt(f -> ((Number) f.get("offset")).intValue()));

                    for (int i = 0; i < sortedFields.size(); i++) {
                        Map<String, Object> f = sortedFields.get(i);
                        String fieldName = (String) f.get("name");
                        int offset = ((Number) f.get("offset")).intValue();

                        if (offset < 0 || offset >= classSize) continue;

                        int span = 4;
                        if (i + 1 < sortedFields.size()) {
                            int nextOff = ((Number) sortedFields.get(i + 1).get("offset")).intValue();
                            if (nextOff > offset) span = nextOff - offset;
                        } else {
                            span = classSize - offset;
                        }

                        DataType fType = resolveFieldType(fieldName, span, typeVec, typeVec2, typeVec4, typeQAngle, typeHandle, typeQuat, typeUtlStr);
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

            success = true;
            println("[+] SUCCESS: Created " + createdCount + " classes with " + fieldsCount + " typed fields in " + progCat.getPathName());
        } finally {
            dtm.endTransaction(transactionID, success);
        }
    }

    private DataType resolveFieldType(String name, int span, DataType vec, DataType vec2, DataType vec4, DataType qang, DataType handle, DataType quat, DataType utlStr) {
        if (name == null) return Undefined4DataType.dataType;

        if (name.startsWith("m_p") || (name.startsWith("p") && name.length() > 2 && Character.isUpperCase(name.charAt(1)))) {
            return PointerDataType.dataType;
        }
        if (name.startsWith("m_b") || (name.startsWith("b") && name.length() > 2 && Character.isUpperCase(name.charAt(1)))) {
            return BooleanDataType.dataType;
        }
        if (name.startsWith("m_fl") || (name.startswith("fl") && name.length() > 3 && Character.isUpperCase(name.charAt(2)))) {
            return FloatDataType.dataType;
        }
        if (name.startsWith("m_h") && handle != null) {
            return handle;
        }
        if (name.startsWith("m_vec")) {
            if (span == 8 && vec2 != null) return vec2;
            if (span == 16 && vec4 != null) return vec4;
            if (vec != null) return vec;
        }
        if (name.startsWith("m_ang") && qang != null) {
            return qang;
        }
        if ((name.startsWith("m_quat") || name.startsWith("m_q")) && quat != null) {
            return quat;
        }
        if ((name.startsWith("m_sz") || name.startsWith("m_str")) && utlStr != null && span == 8) {
            return utlStr;
        }
        if (name.startsWith("m_i") || name.startsWith("m_n")) {
            if (span == 1) return ByteDataType.dataType;
            if (span == 2) return ShortDataType.dataType;
            if (span == 8) return LongLongDataType.dataType;
            return IntegerDataType.dataType;
        }

        if (span == 1) return ByteDataType.dataType;
        if (span == 2) return ShortDataType.dataType;
        if (span == 4) return IntegerDataType.dataType;
        if (span == 8) return PointerDataType.dataType;
        return Undefined.getUndefinedDataType(Math.min(span, 8));
    }

    private void ensureBaseTypes(DataTypeManager dtm) {
        CategoryPath cat = new CategoryPath("/Source2/BaseTypes");
        if (dtm.getDataType(cat, "Vector") != null) return;

        StructureDataType vec = new StructureDataType(cat, "Vector", 12, dtm);
        vec.replaceAtOffset(0, FloatDataType.dataType, 4, "x", "");
        vec.replaceAtOffset(4, FloatDataType.dataType, 4, "y", "");
        vec.replaceAtOffset(8, FloatDataType.dataType, 4, "z", "");
        dtm.addDataType(vec, DataTypeConflictHandler.REPLACE_HANDLER);

        StructureDataType vec2 = new StructureDataType(cat, "Vector2D", 8, dtm);
        vec2.replaceAtOffset(0, FloatDataType.dataType, 4, "x", "");
        vec2.replaceAtOffset(4, FloatDataType.dataType, 4, "y", "");
        dtm.addDataType(vec2, DataTypeConflictHandler.REPLACE_HANDLER);

        StructureDataType vec4 = new StructureDataType(cat, "Vector4D", 16, dtm);
        vec4.replaceAtOffset(0, FloatDataType.dataType, 4, "x", "");
        vec4.replaceAtOffset(4, FloatDataType.dataType, 4, "y", "");
        vec4.replaceAtOffset(8, FloatDataType.dataType, 4, "z", "");
        vec4.replaceAtOffset(12, FloatDataType.dataType, 4, "w", "");
        dtm.addDataType(vec4, DataTypeConflictHandler.REPLACE_HANDLER);

        StructureDataType qangle = new StructureDataType(cat, "QAngle", 12, dtm);
        qangle.replaceAtOffset(0, FloatDataType.dataType, 4, "pitch", "");
        qangle.replaceAtOffset(4, FloatDataType.dataType, 4, "yaw", "");
        qangle.replaceAtOffset(8, FloatDataType.dataType, 4, "roll", "");
        dtm.addDataType(qangle, DataTypeConflictHandler.REPLACE_HANDLER);

        StructureDataType quat = new StructureDataType(cat, "Quaternion", 16, dtm);
        quat.replaceAtOffset(0, FloatDataType.dataType, 4, "x", "");
        quat.replaceAtOffset(4, FloatDataType.dataType, 4, "y", "");
        quat.replaceAtOffset(8, FloatDataType.dataType, 4, "z", "");
        quat.replaceAtOffset(12, FloatDataType.dataType, 4, "w", "");
        dtm.addDataType(quat, DataTypeConflictHandler.REPLACE_HANDLER);

        StructureDataType handle = new StructureDataType(cat, "CEntityHandle", 4, dtm);
        handle.replaceAtOffset(0, UnsignedIntegerDataType.dataType, 4, "m_Index", "");
        dtm.addDataType(handle, DataTypeConflictHandler.REPLACE_HANDLER);

        StructureDataType utlStr = new StructureDataType(cat, "CUtlString", 8, dtm);
        utlStr.replaceAtOffset(0, PointerDataType.dataType, 8, "m_pszString", "");
        dtm.addDataType(utlStr, DataTypeConflictHandler.REPLACE_HANDLER);
    }

    private File resolveSchemasFile() {
        String[] args = getScriptArgs();
        if (args != null && args.length > 0 && !args[0].isEmpty()) {
            return new File(args[0]);
        }

        // Try standard paths relative to current directory
        File standardPath = new File("versions/profiles/2026-10-25_26144732/schemas.json");
        if (standardPath.exists()) return standardPath;

        // Try finding any profiles directory
        File profilesDir = new File("versions/profiles");
        if (profilesDir.exists() && profilesDir.isDirectory()) {
            File[] subs = profilesDir.listFiles();
            if (subs != null) {
                Arrays.sort(subs, Comparator.comparing(File::getName).reversed());
                for (File sub : subs) {
                    File candidate = new File(sub, "schemas.json");
                    if (candidate.exists()) return candidate;
                }
            }
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
            printerr("Gson parse failed: " + e.getMessage());
            return null;
        }
    }
}
