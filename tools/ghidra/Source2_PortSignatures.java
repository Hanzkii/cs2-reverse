// Source 2 Canonical Function Signature & Symbol Porter
// Automatically imports, names, labels, and bookmarks canonical function signatures from data/function_signatures.json.
//@category Source2
//@description Ports canonical Source 2 function signatures, labels, and bookmarks directly into the active Ghidra program.
//@menupath Tools.Source2.Port Canonical Signatures

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.BookmarkManager;
import ghidra.program.model.listing.BookmarkType;
import ghidra.program.model.listing.CodeUnit;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Listing;
import ghidra.program.model.symbol.Namespace;
import ghidra.program.model.symbol.SourceType;
import ghidra.program.model.symbol.SymbolTable;

import java.io.File;
import java.nio.file.Files;
import java.util.*;

public class Source2_PortSignatures extends GhidraScript {

    @Override
    public void run() throws Exception {
        if (currentProgram == null) {
            printerr("[-] ERROR: No open program in Ghidra.");
            return;
        }

        String rawProgName = currentProgram.getName();
        String progName = normalizeProgramName(rawProgName);
        println("[*] Target Program: " + rawProgName + " (normalized: " + progName + ")");

        File sigsFile = resolveSignaturesFile();
        if (sigsFile == null || !sigsFile.exists()) {
            printerr("[-] ERROR: Could not locate data/function_signatures.json.");
            return;
        }
        println("[*] Loading signatures from: " + sigsFile.getAbsolutePath());

        String jsonContent = new String(Files.readAllBytes(sigsFile.toPath()), "UTF-8");
        List<Map<String, Object>> sigList = parseJsonList(jsonContent);
        if (sigList == null || sigList.isEmpty()) {
            printerr("[-] ERROR: Failed to parse signatures list or list is empty.");
            return;
        }
        println("[*] Total canonical signatures in database: " + sigList.size());

        Address imageBase = currentProgram.getImageBase();
        Listing listing = currentProgram.getListing();
        SymbolTable symbolTable = currentProgram.getSymbolTable();
        BookmarkManager bookmarkManager = currentProgram.getBookmarkManager();

        int txId = currentProgram.startTransaction("Port CS2 Signatures");
        boolean success = false;
        int portedCount = 0;
        int skippedCount = 0;

        try {
            for (Map<String, Object> sig : sigList) {
                String targetBinary = (String) sig.get("binary");
                if (targetBinary == null) continue;

                String normBinary = normalizeProgramName(targetBinary);
                if (!normBinary.equalsIgnoreCase(progName)) {
                    skippedCount++;
                    continue;
                }

                String sigId = (String) sig.get("id");
                String name = (String) sig.get("name");
                String category = (String) sig.get("category");
                String description = (String) sig.get("description");
                String pattern = (String) sig.get("pattern");
                String status = (String) sig.get("status");
                String lastBuild = String.valueOf(sig.get("last_verified_build"));

                String rvaStr = (String) sig.get("current_rva");
                if (rvaStr == null || rvaStr.isEmpty()) {
                    rvaStr = (String) sig.get("baseline_rva");
                }
                if (rvaStr == null || rvaStr.isEmpty()) {
                    printerr("[-] Skipping " + sigId + ": No valid RVA found.");
                    continue;
                }

                long rvaOffset = parseHexOrDec(rvaStr);
                Address targetAddr = imageBase.add(rvaOffset);

                // Ensure disassembly exists at target address
                if (listing.getInstructionAt(targetAddr) == null) {
                    try {
                        disassemble(targetAddr);
                    } catch (Exception ex) {
                        // Disassembly warning - proceed with function creation
                    }
                }

                // Resolve or create function
                Function func = getFunctionAt(targetAddr);
                if (func == null) {
                    try {
                        func = createFunction(targetAddr, null);
                    } catch (Exception ex) {
                        println("[!] Notice: createFunction at " + targetAddr + ": " + ex.getMessage());
                        func = getFunctionAt(targetAddr);
                    }
                }

                // Apply function name and namespace
                if (func != null) {
                    if (name.contains("::")) {
                        int lastSep = name.lastIndexOf("::");
                        String nsPath = name.substring(0, lastSep);
                        String methodName = name.substring(lastSep + 2);

                        Namespace parentNs = resolveNamespace(symbolTable, nsPath);
                        try {
                            func.setParentNamespace(parentNs);
                            func.setName(methodName, SourceType.USER_DEFINED);
                        } catch (Exception ex) {
                            String safeFallback = name.replace("::", "_");
                            func.setName(safeFallback, SourceType.USER_DEFINED);
                        }
                    } else {
                        try {
                            func.setName(name, SourceType.USER_DEFINED);
                        } catch (Exception ex) {
                            println("[!] Warning setting name '" + name + "': " + ex.getMessage());
                        }
                    }
                }

                // Set Plate Comment with rich metadata
                StringBuilder plate = new StringBuilder();
                plate.append("=================================================================\n");
                plate.append(" Source 2 Canonical Function Signature\n");
                plate.append(" ID:          ").append(sigId).append("\n");
                plate.append(" Symbol:      ").append(name).append("\n");
                plate.append(" Category:    ").append(category).append("\n");
                plate.append(" Description: ").append(description).append("\n");
                plate.append(" Status:      ").append(status).append(" (Build: ").append(lastBuild).append(")\n");
                plate.append(" Pattern:     ").append(pattern).append("\n");
                plate.append("=================================================================");
                setPlateComment(targetAddr, plate.toString());

                // Set Ghidra Bookmark
                String bookmarkComment = String.format("[%s] %s: %s", category, name, description);
                bookmarkManager.setBookmark(targetAddr, BookmarkType.INFO, "CS2 Canonical Signature", bookmarkComment);

                println(String.format("  [+] Ported %-30s at %s (RVA: %s)", name, targetAddr, rvaStr));
                portedCount++;
            }

            success = true;
            println(String.format("[+] Successfully ported %d canonical signatures to %s (%d skipped)", 
                portedCount, rawProgName, skippedCount));
        } catch (Exception e) {
            printerr("[-] Error during signature porting: " + e.getMessage());
            e.printStackTrace();
        } finally {
            currentProgram.endTransaction(txId, success);
        }
    }

    private String normalizeProgramName(String name) {
        if (name == null) return "";
        String clean = name.toLowerCase();
        // Remove project folder path if included
        if (clean.contains("/")) {
            clean = clean.substring(clean.lastIndexOf('/') + 1);
        }
        if (clean.contains("\\")) {
            clean = clean.substring(clean.lastIndexOf('\\') + 1);
        }
        // Remove trailing version numbers (e.g. client.dll.0 -> client.dll)
        if (clean.matches(".+\\.[0-9]+$")) {
            clean = clean.substring(0, clean.lastIndexOf('.'));
        }
        return clean;
    }

    private Namespace resolveNamespace(SymbolTable st, String fullPath) {
        String[] parts = fullPath.split("::");
        Namespace current = currentProgram.getGlobalNamespace();
        for (String part : parts) {
            if (part.isEmpty()) continue;
            try {
                current = st.getOrCreateNameSpace(current, part, SourceType.USER_DEFINED);
            } catch (Exception e) {
                // Return current if namespace resolution fails
                break;
            }
        }
        return current;
    }

    private long parseHexOrDec(String s) {
        String trimmed = s.trim();
        if (trimmed.startsWith("0x") || trimmed.startsWith("0X")) {
            return Long.parseLong(trimmed.substring(2), 16);
        }
        return Long.parseLong(trimmed);
    }

    private File resolveSignaturesFile() {
        String[] args = getScriptArgs();
        if (args != null && args.length > 0 && !args[0].isEmpty()) {
            File f = new File(args[0]);
            if (f.exists()) return f;
        }

        // Try relative locations
        String[] candidates = {
            "data/function_signatures.json",
            "../data/function_signatures.json",
            "../../data/function_signatures.json"
        };
        for (String c : candidates) {
            File f = new File(c);
            if (f.exists()) return f.getAbsoluteFile();
        }

        // Try looking upwards from script location
        try {
            generic.jar.ResourceFile srcFile = getSourceFile();
            if (srcFile != null) {
                File scriptFile = srcFile.getFile(false);
                if (scriptFile != null && scriptFile.getParentFile() != null) {
                    File cand = new File(scriptFile.getParentFile().getParentFile(), "data/function_signatures.json");
                    if (cand.exists()) return cand;
                }
            }
        } catch (Exception ignored) {}

        return null;
    }

    @SuppressWarnings("unchecked")
    private List<Map<String, Object>> parseJsonList(String json) {
        try {
            Class<?> gsonClass = Class.forName("com.google.gson.Gson");
            Object gson = gsonClass.getDeclaredConstructor().newInstance();
            java.lang.reflect.Method fromJson = gsonClass.getMethod("fromJson", String.class, Class.class);
            return (List<Map<String, Object>>) fromJson.invoke(gson, json, List.class);
        } catch (Exception e) {
            printerr("Gson parse failed: " + e.getMessage());
            return null;
        }
    }
}
