// Source 2 Base Types Initializer
// Registers foundational Source 2 math, memory, and container types in Ghidra's DataTypeManager.
//@category Source2
//@description Registers foundational Source 2 primitive types (Vector, QAngle, CTransform, CEntityHandle, CUtlVector)
//@menupath Tools.Source2.Create Base Types

import ghidra.app.script.GhidraScript;
import ghidra.program.model.data.*;

public class Source2_BaseTypes extends GhidraScript {

    @Override
    public void run() throws Exception {
        if (currentProgram == null) {
            printerr("ERROR: No open program in Ghidra.");
            return;
        }

        DataTypeManager dtm = currentProgram.getDataTypeManager();
        int transactionID = dtm.startTransaction("Create Source 2 Base Types");
        boolean success = false;

        try {
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
            quat.replaceAtOffset(12, FloatDataType.dataType, 4, "w", "W scalar component");
            dtm.addDataType(quat, DataTypeConflictHandler.REPLACE_HANDLER);

            // CTransform (32 bytes aligned)
            StructureDataType transform = new StructureDataType(cat, "CTransform", 32, dtm);
            transform.replaceAtOffset(0, quat, 16, "rot", "Rodrigues rotation quaternion");
            transform.replaceAtOffset(16, vec, 12, "pos", "Translation world position");
            transform.replaceAtOffset(28, FloatDataType.dataType, 4, "pad", "16-byte alignment pad");
            dtm.addDataType(transform, DataTypeConflictHandler.REPLACE_HANDLER);

            // CEntityHandle (4 bytes bitfield: index & 0x7FFF, serial >> 15)
            StructureDataType handle = new StructureDataType(cat, "CEntityHandle", 4, dtm);
            handle.replaceAtOffset(0, UnsignedIntegerDataType.dataType, 4, "m_Index", "Bits 0-14: Entity Index | Bits 15-31: Serial Number");
            dtm.addDataType(handle, DataTypeConflictHandler.REPLACE_HANDLER);

            // CUtlString (8 bytes pointer)
            StructureDataType utlStr = new StructureDataType(cat, "CUtlString", 8, dtm);
            utlStr.replaceAtOffset(0, PointerDataType.dataType, 8, "m_pszString", "Null-terminated char buffer pointer");
            dtm.addDataType(utlStr, DataTypeConflictHandler.REPLACE_HANDLER);

            // CUtlSymbolLarge (8 bytes pointer)
            StructureDataType utlSym = new StructureDataType(cat, "CUtlSymbolLarge", 8, dtm);
            utlSym.replaceAtOffset(0, PointerDataType.dataType, 8, "m_pString", "Interned string table pointer");
            dtm.addDataType(utlSym, DataTypeConflictHandler.REPLACE_HANDLER);

            // CUtlStringToken (4 bytes MurmurHash2 hash)
            StructureDataType utlTok = new StructureDataType(cat, "CUtlStringToken", 4, dtm);
            utlTok.replaceAtOffset(0, UnsignedIntegerDataType.dataType, 4, "m_nHashCode", "MurmurHash2 32-bit token");
            dtm.addDataType(utlTok, DataTypeConflictHandler.REPLACE_HANDLER);

            // CUtlVector_void (16 bytes)
            StructureDataType utlVec = new StructureDataType(cat, "CUtlVector_void", 16, dtm);
            utlVec.replaceAtOffset(0, IntegerDataType.dataType, 4, "m_Size", "Current element count");
            utlVec.replaceAtOffset(4, IntegerDataType.dataType, 4, "m_Capacity", "Allocated element capacity");
            utlVec.replaceAtOffset(8, PointerDataType.dataType, 8, "m_pElements", "Pointer to array memory");
            dtm.addDataType(utlVec, DataTypeConflictHandler.REPLACE_HANDLER);

            success = true;
            println("[+] Successfully registered 11 Source 2 base types in /Source2/BaseTypes");
        } finally {
            dtm.endTransaction(transactionID, success);
        }
    }
}
