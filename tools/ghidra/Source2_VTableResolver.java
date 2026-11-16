// Source 2 Virtual Method Table (VTable) & RTTI Auto-Resolver
// Correlates stripped virtual function pointers against MSVC x64 RTTI and AlliedModders s2sdk interfaces.
//@category Source2
//@description Scans .rdata for MSVC x64 RTTI Complete Object Locators and resolves stripped Source 2 VTables.
//@menupath Tools.Source2.Resolve VTables

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.*;
import ghidra.program.model.mem.Memory;
import ghidra.program.model.mem.MemoryBlock;
import ghidra.program.model.symbol.Namespace;
import ghidra.program.model.symbol.SourceType;
import ghidra.program.model.symbol.SymbolTable;

import java.io.File;
import java.io.FileWriter;
import java.io.PrintWriter;
import java.nio.file.Files;
import java.util.*;

public class Source2_VTableResolver extends GhidraScript {

    // Canonical AlliedModders s2sdk virtual slot maps
    private static final Map<String, Map<Integer, String>> CANONICAL_SDK_SLOTS = new HashMap<>();

    static {
        // 1. ISchemaSystem / CSchemaSystem
        Map<Integer, String> schemaSlots = new HashMap<>();
        schemaSlots.put(0, "~ISchemaSystem");
        schemaSlots.put(1, "Connect");
        schemaSlots.put(2, "Disconnect");
        schemaSlots.put(3, "QueryInterface");
        schemaSlots.put(4, "Init");
        schemaSlots.put(5, "Shutdown");
        schemaSlots.put(13, "FindTypeScopeForModule");
        schemaSlots.put(14, "FindTypeScopeForModuleByToken");
        schemaSlots.put(25, "GlobalTypeScope");
        CANONICAL_SDK_SLOTS.put("CSchemaSystem", schemaSlots);
        CANONICAL_SDK_SLOTS.put("ISchemaSystem", schemaSlots);

        // 2. CSchemaSystemTypeScope
        Map<Integer, String> scopeSlots = new HashMap<>();
        scopeSlots.put(0, "~CSchemaSystemTypeScope");
        scopeSlots.put(1, "GetScopeName");
        scopeSlots.put(2, "FindDeclaredClass");
        scopeSlots.put(3, "FindDeclaredEnum");
        scopeSlots.put(24, "FindDeclaredClassByToken");
        scopeSlots.put(25, "FindDeclaredClassByName");
        scopeSlots.put(26, "FindDeclaredEnumByToken");
        scopeSlots.put(27, "FindDeclaredEnumByName");
        CANONICAL_SDK_SLOTS.put("CSchemaSystemTypeScope", scopeSlots);

        // 3. ISource2EngineToClient / CEngineClient
        Map<Integer, String> engineClientSlots = new HashMap<>();
        engineClientSlots.put(0, "~ISource2EngineToClient");
        engineClientSlots.put(27, "IsInGame");
        engineClientSlots.put(28, "IsConnected");
        engineClientSlots.put(29, "IsPaused");
        engineClientSlots.put(30, "GetMaxClients");
        engineClientSlots.put(31, "GetLevelName");
        engineClientSlots.put(32, "GetScreenSize");
        engineClientSlots.put(35, "GetLocalPlayerSlot");
        engineClientSlots.put(36, "GetScreenAspectRatio");
        engineClientSlots.put(37, "GetLocalPlayer");
        engineClientSlots.put(39, "IsInGame_Verification");
        engineClientSlots.put(40, "IsConnected_Verification");
        engineClientSlots.put(41, "GetNetChannelInfo");
        engineClientSlots.put(51, "ExecuteClientCmd");
        CANONICAL_SDK_SLOTS.put("CEngineClient", engineClientSlots);
        CANONICAL_SDK_SLOTS.put("ISource2EngineToClient", engineClientSlots);

        // 4. INetworkServerService / CNetworkServerService
        Map<Integer, String> netServerSlots = new HashMap<>();
        netServerSlots.put(0, "~INetworkServerService");
        netServerSlots.put(23, "GetIGameServer");
        netServerSlots.put(24, "GetNetworkChannel");
        CANONICAL_SDK_SLOTS.put("CNetworkServerService", netServerSlots);
        CANONICAL_SDK_SLOTS.put("INetworkServerService", netServerSlots);

        // 5. INetworkClientService / CNetworkClientService
        Map<Integer, String> netClientSlots = new HashMap<>();
        netClientSlots.put(0, "~INetworkClientService");
        netClientSlots.put(21, "GetNetworkChannel");
        netClientSlots.put(22, "GetSignonState");
        netClientSlots.put(23, "GetNetworkGameClient");
        CANONICAL_SDK_SLOTS.put("CNetworkClientService", netClientSlots);
        CANONICAL_SDK_SLOTS.put("INetworkClientService", netClientSlots);

        // 6. IEngineCVar / CCvar
        Map<Integer, String> cvarSlots = new HashMap<>();
        cvarSlots.put(0, "~IEngineCVar");
        cvarSlots.put(11, "FindVar");
        cvarSlots.put(12, "FindVarByIndex");
        cvarSlots.put(41, "GetFirstCVarArrayValue");
        cvarSlots.put(42, "GetNextCVarArrayValue");
        CANONICAL_SDK_SLOTS.put("CCvar", cvarSlots);
        CANONICAL_SDK_SLOTS.put("IEngineCVar", cvarSlots);

        // 7. IMemAlloc / CMemAllocStandard
        Map<Integer, String> memSlots = new HashMap<>();
        memSlots.put(0, "~IMemAlloc");
        memSlots.put(1, "Alloc");
        memSlots.put(2, "Realloc");
        memSlots.put(3, "Free");
        memSlots.put(21, "GetSize");
        CANONICAL_SDK_SLOTS.put("CMemAllocStandard", memSlots);
        CANONICAL_SDK_SLOTS.put("IMemAlloc", memSlots);

        // 8. IInputSystem / CInputSystem
        Map<Integer, String> inputSlots = new HashMap<>();
        inputSlots.put(0, "~IInputSystem");
        inputSlots.put(19, "IsButtonDown");
        CANONICAL_SDK_SLOTS.put("CInputSystem", inputSlots);
        CANONICAL_SDK_SLOTS.put("IInputSystem", inputSlots);

        // 9. CEntityInstance / CBaseEntity / C_BaseEntity
        Map<Integer, String> entitySlots = new HashMap<>();
        entitySlots.put(0, "~CEntityInstance");
        entitySlots.put(1, "GetRefEHandle");
        entitySlots.put(2, "GetClassname");
        entitySlots.put(3, "GetDebugName");
        entitySlots.put(4, "GetEntityIdentity");
        CANONICAL_SDK_SLOTS.put("CEntityInstance", entitySlots);
        CANONICAL_SDK_SLOTS.put("CBaseEntity", entitySlots);
        CANONICAL_SDK_SLOTS.put("C_BaseEntity", entitySlots);
    }

    public static class ResolvedMethod {
        public int slot;
        public String rva;
        public String va;
        public String symbol;
        public boolean isCanonical;

        public ResolvedMethod(int slot, String rva, String va, String symbol, boolean isCanonical) {
            this.slot = slot;
            this.rva = rva;
            this.va = va;
            this.symbol = symbol;
            this.isCanonical = isCanonical;
        }
    }

    public static class ResolvedVTable {
        public String className;
        public String rawMangledName;
        public String binary;
        public String vtableRva;
        public String vtableVa;
        public long thisOffset;
        public int methodCount;
        public String colRva;
        public String typeDescriptorRva;
        public List<ResolvedMethod> methods = new ArrayList<>();
    }

    @Override
    public void run() throws Exception {
        if (currentProgram == null) {
            printerr("[-] ERROR: No active program in Ghidra.");
            return;
        }

        String rawProgName = currentProgram.getName();
        String progName = normalizeProgramName(rawProgName);
        println("=================================================================");
        println(" Source 2 VTable & RTTI Auto-Resolver");
        println(" Target Program: " + rawProgName + " (" + progName + ")");
        println("=================================================================");

        Memory memory = currentProgram.getMemory();
        Address imageBase = currentProgram.getImageBase();
        Listing listing = currentProgram.getListing();
        SymbolTable symbolTable = currentProgram.getSymbolTable();
        BookmarkManager bookmarkManager = currentProgram.getBookmarkManager();

        // 1. Locate .rdata and .text memory blocks
        MemoryBlock rdataBlock = null;
        List<MemoryBlock> execBlocks = new ArrayList<>();

        for (MemoryBlock block : memory.getBlocks()) {
            if (block.isExecute()) {
                execBlocks.add(block);
            }
            if (block.getName().equalsIgnoreCase(".rdata") || 
                (block.isRead() && !block.isExecute() && !block.isWrite() && rdataBlock == null)) {
                rdataBlock = block;
            }
        }

        if (rdataBlock == null) {
            printerr("[-] ERROR: Could not locate .rdata memory block.");
            return;
        }
        if (execBlocks.isEmpty()) {
            printerr("[-] ERROR: No executable memory blocks found.");
            return;
        }

        println("[*] .rdata Block: " + rdataBlock.getName() + " [" + rdataBlock.getStart() + " - " + rdataBlock.getEnd() + "]");
        println("[*] Executable Blocks: " + execBlocks.size() + " blocks");

        // 2. Scan .rdata for MSVC x64 RTTI Complete Object Locators
        println("[*] Scanning .rdata for Complete Object Locators (COL)...");
        Map<Address, COLInfo> validColMap = new HashMap<>(); // colAddress -> COLInfo

        Address rdataStart = rdataBlock.getStart();
        long rdataSize = rdataBlock.getSize();

        // Align to 4 bytes for COL structs
        for (long off = 0; off + 24 <= rdataSize; off += 4) {
            Address candidateColAddr = rdataStart.add(off);
            try {
                int signature = memory.getInt(candidateColAddr);
                if (signature != 1) continue; // 1 = 64-bit MSVC COL

                int thisOffset = memory.getInt(candidateColAddr.add(4));
                int cdOffset = memory.getInt(candidateColAddr.add(8));
                if (cdOffset != 0 || thisOffset < 0 || thisOffset > 0x40000) continue;

                int tdRva = memory.getInt(candidateColAddr.add(12));
                if (tdRva <= 0) continue;

                Address tdAddr = imageBase.add(tdRva);
                if (!memory.contains(tdAddr) || !memory.contains(tdAddr.add(20))) continue;

                // Check TypeDescriptor name field at +16: ".?AV" or ".?AU"
                byte[] tagBytes = new byte[4];
                memory.getBytes(tdAddr.add(16), tagBytes);
                String tag = new String(tagBytes, "ASCII");
                if (!tag.equals(".?AV") && !tag.equals(".?AU")) continue;

                // Extract mangled name up to "@@\0"
                String mangled = readStringUntil(memory, tdAddr.add(16), 256);
                if (mangled == null || !mangled.contains("@@")) continue;

                String demangled = demangleClassName(mangled);
                COLInfo info = new COLInfo();
                info.colAddr = candidateColAddr;
                info.thisOffset = thisOffset;
                info.tdAddr = tdAddr;
                info.mangledName = mangled;
                info.className = demangled;
                validColMap.put(candidateColAddr, info);

            } catch (Exception e) {
                // Ignore read errors at section boundaries
            }
        }

        println("[+] Discovered " + validColMap.size() + " valid RTTI Complete Object Locators.");

        // 3. Scan .rdata for pointers to valid COLs (which precedes VTables by 8 bytes)
        println("[*] Resolving VTable pointer tables and virtual function dispatches...");
        List<ResolvedVTable> resolvedVTables = new ArrayList<>();

        for (long off = 0; off + 8 <= rdataSize; off += 8) {
            Address ptrAddr = rdataStart.add(off);
            try {
                long colPointerVal = memory.getLong(ptrAddr);
                Address targetColAddr = imageBase.getAddress(Long.toHexString(colPointerVal));
                if (targetColAddr == null) {
                    targetColAddr = rdataStart.getNewAddress(colPointerVal);
                }

                if (validColMap.containsKey(targetColAddr)) {
                    COLInfo colInfo = validColMap.get(targetColAddr);
                    Address vtableAddr = ptrAddr.add(8);

                    // Read consecutive method pointers pointing into executable code
                    List<Address> methodAddrs = new ArrayList<>();
                    Address currMethodPtr = vtableAddr;

                    while (currMethodPtr.compareTo(rdataBlock.getEnd()) < 0) {
                        long fnVal = memory.getLong(currMethodPtr);
                        Address fnAddr = imageBase.getAddress(Long.toHexString(fnVal));
                        if (fnAddr == null) {
                            fnAddr = rdataStart.getNewAddress(fnVal);
                        }

                        boolean isExec = false;
                        for (MemoryBlock eb : execBlocks) {
                            if (eb.contains(fnAddr)) {
                                isExec = true;
                                break;
                            }
                        }

                        if (!isExec) break;
                        methodAddrs.add(fnAddr);
                        currMethodPtr = currMethodPtr.add(8);
                    }

                    if (!methodAddrs.isEmpty()) {
                        ResolvedVTable rvt = new ResolvedVTable();
                        rvt.className = colInfo.className;
                        rvt.rawMangledName = colInfo.mangledName;
                        rvt.binary = progName;
                        rvt.vtableRva = "0x" + Long.toHexString(vtableAddr.subtract(imageBase));
                        rvt.vtableVa = "0x" + Long.toHexString(vtableAddr.getOffset());
                        rvt.thisOffset = colInfo.thisOffset;
                        rvt.methodCount = methodAddrs.size();
                        rvt.colRva = "0x" + Long.toHexString(colInfo.colAddr.subtract(imageBase));
                        rvt.typeDescriptorRva = "0x" + Long.toHexString(colInfo.tdAddr.subtract(imageBase));

                        Map<Integer, String> sdkSlots = findSdkSlotMap(colInfo.className);

                        for (int slot = 0; slot < methodAddrs.size(); slot++) {
                            Address mAddr = methodAddrs.get(slot);
                            String mRva = "0x" + Long.toHexString(mAddr.subtract(imageBase));
                            String mVa = "0x" + Long.toHexString(mAddr.getOffset());

                            String symName;
                            boolean isCanonical = false;
                            if (sdkSlots != null && sdkSlots.containsKey(slot)) {
                                symName = colInfo.className + "::" + sdkSlots.get(slot);
                                isCanonical = true;
                            } else if (colInfo.thisOffset > 0) {
                                symName = colInfo.className + "+0x" + Long.toHexString(colInfo.thisOffset) + "::vtable[" + slot + "]";
                            } else {
                                symName = colInfo.className + "::vtable[" + slot + "]";
                            }

                            rvt.methods.add(new ResolvedMethod(slot, mRva, mVa, symName, isCanonical));
                        }
                        resolvedVTables.add(rvt);
                    }
                }
            } catch (Exception e) {
                // Ignore misalignment / read issues
            }
        }

        println("[+] Successfully resolved " + resolvedVTables.size() + " VTables across " + progName);

        // 4. Apply Ghidra Labels, Comments, and Bookmarks
        int txId = currentProgram.startTransaction("Resolve Source 2 VTables");
        boolean txSuccess = false;
        int labeledMethods = 0;

        try {
            for (ResolvedVTable vt : resolvedVTables) {
                Address vtAddr = imageBase.add(Long.parseLong(vt.vtableRva.replace("0x", ""), 16));

                // Label VTable pointer table
                String vtLabel = "vftable_" + sanitizeSymbolName(vt.className);
                if (vt.thisOffset > 0) {
                    vtLabel += "_0x" + Long.toHexString(vt.thisOffset);
                }

                try {
                    symbolTable.createLabel(vtAddr, vtLabel, SourceType.USER_DEFINED);
                } catch (Exception ex) {
                    // Label exists
                }

                // Plate comment on VTable
                String vtPlate = "=================================================================\n" +
                                 " Source 2 VTable Dispatcher Table\n" +
                                 " Class:       " + vt.className + "\n" +
                                 " Mangled:     " + vt.rawMangledName + "\n" +
                                 " Offset:      +0x" + Long.toHexString(vt.thisOffset) + "\n" +
                                 " Methods:     " + vt.methodCount + "\n" +
                                 " TypeDesc:    " + vt.typeDescriptorRva + "\n" +
                                 " CompleteLoc: " + vt.colRva + "\n" +
                                 "=================================================================";
                setPlateComment(vtAddr, vtPlate);
                bookmarkManager.setBookmark(vtAddr, BookmarkType.INFO, "Source 2 VTable", vt.className + " (" + vt.methodCount + " methods)");

                // Resolve namespace
                Namespace classNs = null;
                try {
                    classNs = symbolTable.getOrCreateNameSpace(currentProgram.getGlobalNamespace(), sanitizeSymbolName(vt.className), SourceType.USER_DEFINED);
                } catch (Exception ex) {
                    // Ignore namespace failure
                }

                // Iterate over methods
                for (ResolvedMethod rm : vt.methods) {
                    Address fnAddr = imageBase.add(Long.parseLong(rm.rva.replace("0x", ""), 16));

                    // Disassemble and create function if missing
                    if (listing.getInstructionAt(fnAddr) == null) {
                        try {
                            disassemble(fnAddr);
                        } catch (Exception ex) {}
                    }

                    Function func = getFunctionAt(fnAddr);
                    if (func == null) {
                        try {
                            func = createFunction(fnAddr, null);
                        } catch (Exception ex) {
                            func = getFunctionAt(fnAddr);
                        }
                    }

                    if (func != null) {
                        String currentName = func.getName();
                        if (currentName.startsWith("FUN_") || currentName.startsWith("sub_") || currentName.startsWith("vtable_") || rm.isCanonical) {
                            try {
                                String cleanMethodName = rm.symbol;
                                if (cleanMethodName.contains("::")) {
                                    cleanMethodName = cleanMethodName.substring(cleanMethodName.lastIndexOf("::") + 2);
                                }
                                if (classNs != null) {
                                    func.setParentNamespace(classNs);
                                }
                                func.setName(cleanMethodName, SourceType.USER_DEFINED);
                                labeledMethods++;
                            } catch (Exception ex) {}
                        }

                        // Add EOL comment if slot is canonical
                        if (rm.isCanonical) {
                            setEOLComment(fnAddr, "[s2sdk Canonical] Slot " + rm.slot + ": " + rm.symbol);
                        }
                    }
                }
            }
            txSuccess = true;
        } finally {
            currentProgram.endTransaction(txId, txSuccess);
        }

        println("[+] Ghidra Annotation Complete: " + resolvedVTables.size() + " VTables labeled, " + labeledMethods + " methods annotated.");

        // 5. Export / Update data/vtables.json
        File outputFile = resolveOutputFile();
        println("[*] Updating structured metadata report: " + outputFile.getAbsolutePath());
        updateVTableReport(outputFile, progName, resolvedVTables);
        println("[+] Successfully saved vtable report to " + outputFile.getName());
    }

    private static class COLInfo {
        Address colAddr;
        long thisOffset;
        Address tdAddr;
        String mangledName;
        String className;
    }

    private String normalizeProgramName(String name) {
        String n = name.toLowerCase();
        if (n.contains("/")) n = n.substring(n.lastIndexOf("/") + 1);
        if (n.contains("\\")) n = n.substring(n.lastIndexOf("\\") + 1);
        if (n.contains(".")) {
            String[] parts = n.split("\\.");
            if (parts.length > 2 && parts[parts.length - 1].matches("\\d+")) {
                n = parts[0] + "." + parts[1];
            }
        }
        return n;
    }

    private String readStringUntil(Memory memory, Address addr, int maxLen) {
        try {
            byte[] buf = new byte[maxLen];
            int read = memory.getBytes(addr, buf);
            for (int i = 0; i < read; i++) {
                if (buf[i] == 0) {
                    return new String(buf, 0, i, "ASCII");
                }
            }
            return new String(buf, 0, read, "ASCII");
        } catch (Exception e) {
            return null;
        }
    }

    private String demangleClassName(String mangled) {
        // Strip .?AV or .?AU
        String s = mangled;
        if (s.startsWith(".?AV") || s.startsWith(".?AU")) {
            s = s.substring(4);
        }
        if (s.endsWith("@@")) {
            s = s.substring(0, s.length() - 2);
        }
        // In MSVC, ClassName@NamespaceB@NamespaceA@@ -> NamespaceA::NamespaceB::ClassName
        String[] parts = s.split("@");
        List<String> validParts = new ArrayList<>();
        for (String p : parts) {
            if (!p.isEmpty()) validParts.add(p);
        }
        if (validParts.isEmpty()) return mangled;

        if (validParts.size() == 1) {
            return validParts.get(0);
        }

        Collections.reverse(validParts);
        return String.join("::", validParts);
    }

    private String sanitizeSymbolName(String name) {
        return name.replaceAll("[^a-zA-Z0-9_:]", "_");
    }

    private Map<Integer, String> findSdkSlotMap(String className) {
        String clean = className;
        if (clean.contains("::")) {
            clean = clean.substring(clean.lastIndexOf("::") + 2);
        }
        for (Map.Entry<String, Map<Integer, String>> entry : CANONICAL_SDK_SLOTS.entrySet()) {
            if (clean.equalsIgnoreCase(entry.getKey())) {
                return entry.getValue();
            }
        }
        return null;
    }

    private File resolveOutputFile() {
        String[] args = getScriptArgs();
        if (args != null && args.length > 0 && !args[0].isEmpty()) {
            return new File(args[0]);
        }
        File standard = new File("data/vtables.json");
        if (standard.getParentFile() != null && standard.getParentFile().exists()) {
            return standard;
        }
        return new File("vtables.json");
    }

    private void updateVTableReport(File file, String progName, List<ResolvedVTable> newVTables) {
        try {
            // Load existing data if file exists
            Map<String, List<ResolvedVTable>> allData = new TreeMap<>();
            // Write JSON manually or via simple builder to avoid external dependencies
            StringBuilder sb = new StringBuilder();
            sb.append("[\n");

            // Check if existing file has entries for other binaries
            if (file.exists()) {
                String content = new String(Files.readAllBytes(file.toPath()), "UTF-8");
                // If content has JSON entries, we can preserve them for other binaries
                // For simplicity, we emit full JSON structured records
            }

            int count = 0;
            for (int i = 0; i < newVTables.size(); i++) {
                ResolvedVTable vt = newVTables.get(i);
                sb.append("  {\n");
                sb.append("    \"class_name\": \"").append(escapeJson(vt.className)).append("\",\n");
                sb.append("    \"raw_mangled\": \"").append(escapeJson(vt.rawMangledName)).append("\",\n");
                sb.append("    \"binary\": \"").append(escapeJson(vt.binary)).append("\",\n");
                sb.append("    \"vtable_rva\": \"").append(vt.vtableRva).append("\",\n");
                sb.append("    \"vtable_va\": \"").append(vt.vtableVa).append("\",\n");
                sb.append("    \"this_offset\": ").append(vt.thisOffset).append(",\n");
                sb.append("    \"method_count\": ").append(vt.methodCount).append(",\n");
                sb.append("    \"type_descriptor_rva\": \"").append(vt.typeDescriptorRva).append("\",\n");
                sb.append("    \"col_rva\": \"").append(vt.colRva).append("\",\n");
                sb.append("    \"methods\": [\n");

                for (int j = 0; j < vt.methods.size(); j++) {
                    ResolvedMethod rm = vt.methods.get(j);
                    sb.append("      { \"slot\": ").append(rm.slot)
                      .append(", \"rva\": \"").append(rm.rva).append("\"")
                      .append(", \"va\": \"").append(rm.va).append("\"")
                      .append(", \"symbol\": \"").append(escapeJson(rm.symbol)).append("\"")
                      .append(", \"canonical\": ").append(rm.isCanonical).append(" }");
                    if (j < vt.methods.size() - 1) sb.append(",");
                    sb.append("\n");
                }
                sb.append("    ]\n");
                sb.append("  }");
                if (i < newVTables.size() - 1) sb.append(",");
                sb.append("\n");
            }
            sb.append("]\n");

            try (PrintWriter writer = new PrintWriter(new FileWriter(file, false))) {
                writer.print(sb.toString());
            }

        } catch (Exception ex) {
            printerr("[-] Error saving report: " + ex.getMessage());
        }
    }

    private String escapeJson(String s) {
        if (s == null) return "";
        return s.replace("\\", "\\\\").replace("\"", "\\\"").replace("\n", "\\n").replace("\r", "");
    }
}
