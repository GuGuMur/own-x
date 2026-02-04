enum TokenKind {
    Keyword,
    Identifier,
    StringLiteral,
    Seperator,
    Operator,
    EOF,
}
interface Token {
    kind: TokenKind;
    text: string;
}
class Tokenizer {
    private tokens: Token[];
    private pos: number = 0;

    constructor(tokens: Token[]) {
        this.tokens = tokens;
    }
    next(): Token {
        if (this.pos <= this.tokens.length) {
            return this.tokens[this.pos++];
        } else {
            //如果已经到了末尾，总是返回EOF
            return this.tokens[this.pos];
        }
    }
    position(): number {
        return this.pos;
    }
    traceBack(newPos: number): void {
        this.pos = newPos;
    }
}

abstract class AstNode {
    //打印对象信息，prefix是前面填充的字符串，通常用于缩进显示
    public abstract dump(prefix: string): void;
}

abstract class Statement extends AstNode {
    static isStatementNode(node: any): node is Statement {
        if (!node) {
            return false;
        } else {
            return true;
        }
    }
}