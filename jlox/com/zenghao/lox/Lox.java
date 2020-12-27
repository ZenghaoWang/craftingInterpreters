package com.zenghao.lox;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.nio.charset.Charset;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.List;

public class Lox {
  static boolean hadError = false;

  public static void main(String[] args) throws IOException {
    switch (args.length) {
      case 0:
        // Run the interactive prompt
        runPrompt();
        break;
      case 1:
        // Run the provided script
        runFile(args[0]);
        break;
      default:
        System.out.println("Usage: jlox [script]");
        System.exit(64);
    }
  }

  /**
   * Run a script.
   * 
   * @param path to script
   * @throws IOException
   */
  private static void runFile(String path) throws IOException {
    byte[] bytes = Files.readAllBytes(Paths.get(path));
    run(new String(bytes, Charset.defaultCharset()));

    // Set to true by error handling function
    if (hadError) {
      System.exit(65);
    }
  }

  /**
   * Interactive REPL.
   */
  private static void runPrompt() throws IOException {
    InputStreamReader input = new InputStreamReader(System.in);
    BufferedReader reader = new BufferedReader(input);

    // Continuously read and execute user input
    while (true) {
      System.out.print("> ");
      // Get user input
      String line = reader.readLine();
      if (line == null) {
        // Exit loop if user inputs ctrl+D or otherwise terminates program
        break;
      }

      run(line);

      // Reset flag so that a user mistake does not kill the whole session
      hadError = false;
    }
  }

  /**
   * Run a source code string.
   * 
   * @param source
   */
  private static void run(String source) {
    Scanner scanner = new Scanner(source);
    List<Token> tokens = scanner.scanTokens();
  }

  static void error(int line, String message) {
    report(line, "", message);
  }

  private static void report(int line, String where, String message) {
    System.err.println("[line " + line + "] Error" + where + ": " + message);
    hadError = true;
  }
}
