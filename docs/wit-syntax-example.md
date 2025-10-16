# WIT Syntax Highlighting Example

This page demonstrates WIT (WebAssembly Interface Types) syntax highlighting in VitePress.

## Basic Package Definition

```wit
package example:component@0.1.0;

interface types {
    /// A person with name and age
    record person {
        name: string,
        age: u32,
    }
    
    /// Get a greeting for a person
    greet: func(p: person) -> string;
}

world hello {
    export types;
}
```

## Resource Example

```wit
package example:resources@1.0.0;

interface database {
    /// A database connection resource
    resource connection {
        /// Create a new connection
        constructor(url: string);
        
        /// Execute a query
        query: func(sql: string) -> result<list<string>, string>;
        
        /// Close the connection
        close: func();
    }
}
```

## Async Operations

```wit
package example:async@0.1.0;

interface io {
    use wasi:io/streams@0.2.0.{input-stream, output-stream};
    
    /// Read data asynchronously
    read-async: func(stream: borrow<input-stream>) -> future<result<list<u8>, error-code>>;
    
    /// Write data asynchronously
    write-async: func(stream: borrow<output-stream>, data: list<u8>) -> future<result<_, error-code>>;
}
```

## Complex Types

```wit
package example:types@0.1.0;

interface types {
    /// Boolean values
    type flag = bool;
    
    /// Variant type
    variant color {
        rgb(u8, u8, u8),
        rgba(u8, u8, u8, u8),
        named(string),
    }
    
    /// Flags for permissions
    flags permissions {
        read,
        write,
        execute,
    }
    
    /// Enum for log levels
    enum log-level {
        debug,
        info,
        warning,
        error,
    }
    
    /// Option type
    type maybe-string = option<string>;
    
    /// Result type
    type io-result = result<u32, string>;
}
```

## World with Imports and Exports

```wit
package example:app@0.2.0;

world application {
    import wasi:filesystem/types@0.2.0;
    import wasi:http/types@0.2.0;
    import wasi:clocks/wall-clock@0.2.0;
    
    export run: func() -> result<_, string>;
}
```

## Feature Gates

```wit
package example:unstable@0.1.0;

interface experimental {
    @since(version = 0.2.0)
    @unstable(feature = new-api)
    new-function: func() -> string;
    
    @deprecated(since = 0.3.0)
    old-function: func() -> string;
}
```

The WIT syntax highlighting is provided by the grammar built from the [vscode-wit](https://github.com/bytecodealliance/vscode-wit) TextMate grammar, making it consistent with VS Code's WIT extension.
