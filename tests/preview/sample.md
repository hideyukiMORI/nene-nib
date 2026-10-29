# NeNe Nib のプレビュー

Markdown の **8 つの構文**をひととおり並べた見本です。*斜体*と**太字**、
それに `行内コード` と [リンクの文字](https://example.com/) が同じ段落に入ります。

- [ ] まだのタスク（押すと本文の `[ ]` が `[x]` になる）
- [x] 済んだタスク

## 見出しの段

### 3 段目の見出し

#### 4 段目は本文と同じ大きさで太字

長い段落は枠の幅で折り返します。The quick brown fox jumps over the lazy dog, and
the paragraph keeps going until it wraps at the edge of the preview frame so that the
line height and the half-line gap between paragraphs can be seen.

## 箇条書き

- 1 つ目の項目
- 2 つ目の項目に `code` と **太字**
  - 入れ子の項目（深さ 2）
    - さらに深い項目（深さ 3）
- 3 つ目の項目

1. 番号つきの 1 つ目
2. 番号つきの 2 つ目で、長い文は折り返して記号の右にそろいます。Wrapped text aligns after the number.
3. 3 つ目

## 引用

> 引用は comment の色で、左に 2 DIP の線が付きます。
>
> > 入れ子の引用は線が 2 本になります。

## コードブロック

```cpp
// コードは等幅で折り返さない
int main()
{
    return std::printf("preview %d\n", 784);
}
```

    字下げのコードブロックも同じ面です

## 水平線と画像と HTML

上の段落。

---

下の段落と画像 ![猫の写真](https://example.com/cat.png) の代わりの文字。

<div class="note">
  HTML ブロックは生の文字のまま
</div>

## 2 桁の番号

1. 1 番目の項目
2. 2 番目の項目
3. 3 番目の項目
4. 4 番目の項目
5. 5 番目の項目
6. 6 番目の項目
7. 7 番目の項目
8. 8 番目の項目
9. 9 番目の項目
10. 10 番目の項目
11. 11 番目の項目
12. 12 番目の項目

- 箇条書きに替わると別のリスト

最後の段落: [Nib](https://github.com/hideyukiMORI/nene-nib) と `:colorscheme` で色が変わります。
