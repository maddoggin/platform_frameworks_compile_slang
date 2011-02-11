/*
 * Copyright 2010, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _FRAMEWORKS_COMPILE_SLANG_SLANG_RS_EXPORT_TYPE_H_  // NOLINT
#define _FRAMEWORKS_COMPILE_SLANG_SLANG_RS_EXPORT_TYPE_H_

#include <list>
#include <set>
#include <string>

#include "clang/AST/Decl.h"
#include "clang/AST/Type.h"

#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/ADT/StringRef.h"

#include "llvm/Support/ManagedStatic.h"

#include "slang_rs_exportable.h"

#define GET_CANONICAL_TYPE(T) \
    (((T) == NULL) ? NULL : (T)->getCanonicalTypeInternal().getTypePtr())
#define UNSAFE_CAST_TYPE(TT, T) \
    static_cast<TT*>(T->getCanonicalTypeInternal().getTypePtr())
#define GET_EXT_VECTOR_ELEMENT_TYPE(T) \
    (((T) == NULL) ? NULL : \
                     GET_CANONICAL_TYPE((T)->getElementType().getTypePtr()))
#define GET_POINTEE_TYPE(T) \
    (((T) == NULL) ? NULL : \
                     GET_CANONICAL_TYPE((T)->getPointeeType().getTypePtr()))
#define GET_CONSTANT_ARRAY_ELEMENT_TYPE(T)  \
    (((T) == NULL) ? NULL : \
                     GET_CANONICAL_TYPE((T)->getElementType().getTypePtr()))
#define DUMMY_RS_TYPE_NAME_PREFIX   "<"
#define DUMMY_RS_TYPE_NAME_POSTFIX  ">"
#define DUMMY_TYPE_NAME_FOR_RS_CONSTANT_ARRAY_TYPE  \
    DUMMY_RS_TYPE_NAME_PREFIX"ConstantArray"DUMMY_RS_TYPE_NAME_POSTFIX

union RSType;

namespace llvm {
  class Type;
}   // namespace llvm

namespace slang {

  class RSContext;

class RSExportType : public RSExportable {
  friend class RSExportElement;
 public:
  typedef enum {
    ExportClassPrimitive,
    ExportClassPointer,
    ExportClassVector,
    ExportClassMatrix,
    ExportClassConstantArray,
    ExportClassRecord
  } ExportClass;

 private:
  ExportClass mClass;
  std::string mName;

  // Cache the result after calling convertToLLVMType() at the first time
  mutable const llvm::Type *mLLVMType;
  // Cache the result after calling convertToSpecType() at the first time
  mutable union RSType *mSpecType;

 protected:
  RSExportType(RSContext *Context,
               ExportClass Class,
               const llvm::StringRef &Name);

  // Let's make it private since there're some prerequisites to call this
  // function.
  //
  // @T was normalized by calling RSExportType::NormalizeType().
  // @TypeName was retrieve from RSExportType::GetTypeName() before calling
  //           this.
  //
  static RSExportType *Create(RSContext *Context,
                              const clang::Type *T,
                              const llvm::StringRef &TypeName);

  static llvm::StringRef GetTypeName(const clang::Type *T);

  // This function convert the RSExportType to LLVM type. Actually, it should be
  // "convert Clang type to LLVM type." However, clang doesn't make this API
  // (lib/CodeGen/CodeGenTypes.h) public, we need to do by ourselves.
  //
  // Once we can get LLVM type, we can use LLVM to get alignment information,
  // allocation size of a given type and structure layout that LLVM used
  // (all of these information are target dependent) without dealing with these
  // by ourselves.
  virtual const llvm::Type *convertToLLVMType() const = 0;
  // Record type may recursively reference its type definition. We need a
  // temporary type setup before the type construction gets done.
  inline void setAbstractLLVMType(const llvm::Type *LLVMType) const {
    mLLVMType = LLVMType;
  }

  virtual union RSType *convertToSpecType() const = 0;
  inline void setSpecTypeTemporarily(union RSType *SpecType) const {
    mSpecType = SpecType;
  }

  virtual ~RSExportType();
 public:
  // This function additionally verifies that the Type T is exportable.
  // If it is not, this function returns false. Otherwise it returns true.
  static bool NormalizeType(const clang::Type *&T,
                            llvm::StringRef &TypeName,
                            clang::Diagnostic *Diags,
                            clang::SourceManager *SM,
                            const clang::VarDecl *VD);
  // @T may not be normalized
  static RSExportType *Create(RSContext *Context, const clang::Type *T);
  static RSExportType *CreateFromDecl(RSContext *Context,
                                      const clang::VarDecl *VD);

  static const clang::Type *GetTypeOfDecl(const clang::DeclaratorDecl *DD);

  inline ExportClass getClass() const { return mClass; }

  inline const llvm::Type *getLLVMType() const {
    if (mLLVMType == NULL)
      mLLVMType = convertToLLVMType();
    return mLLVMType;
  }

  inline const union RSType *getSpecType() const {
    if (mSpecType == NULL)
      mSpecType = convertToSpecType();
    return mSpecType;
  }

  // Return the number of bits necessary to hold the specified RSExportType
  static size_t GetTypeStoreSize(const RSExportType *ET);

  // The size of allocation of specified RSExportType (alignment considered)
  static size_t GetTypeAllocSize(const RSExportType *ET);

  inline const std::string &getName() const { return mName; }

  virtual bool keep();
  virtual bool equals(const RSExportable *E) const;
};  // RSExportType

// Primitive types
class RSExportPrimitiveType : public RSExportType {
  friend class RSExportType;
  friend class RSExportElement;
 public:
  // From graphics/java/android/renderscript/Element.java: Element.DataType
  typedef enum {
    DataTypeIsStruct = -2,
    DataTypeUnknown = -1,

#define ENUM_PRIMITIVE_DATA_TYPE_RANGE(begin_type, end_type)  \
    FirstPrimitiveType = DataType ## begin_type,  \
    LastPrimitiveType = DataType ## end_type,

#define ENUM_RS_MATRIX_DATA_TYPE_RANGE(begin_type, end_type)  \
    FirstRSMatrixType = DataType ## begin_type,  \
    LastRSMatrixType = DataType ## end_type,

#define ENUM_RS_OBJECT_DATA_TYPE_RANGE(begin_type, end_type)  \
    FirstRSObjectType = DataType ## begin_type,  \
    LastRSObjectType = DataType ## end_type,

#define ENUM_RS_DATA_TYPE(type, cname, bits)  \
    DataType ## type,

#include "RSDataTypeEnums.inc"

    DataTypeMax
  } DataType;

  // From graphics/java/android/renderscript/Element.java: Element.DataKind
  typedef enum {
    DataKindUnknown = -1
#define ENUM_RS_DATA_KIND(kind) \
    , DataKind ## kind
#include "RSDataKindEnums.inc"
  } DataKind;

 private:
  // NOTE: There's no any instance of RSExportPrimitiveType which mType
  // is of the value DataTypeRSMatrix*. DataTypeRSMatrix* enumeration here is
  // only for RSExportPrimitiveType::GetRSObjectType to *recognize* the struct
  // rs_matrix{2x2, 3x3, 4x4}. These matrix type are represented as
  // RSExportMatrixType.
  DataType mType;
  DataKind mKind;
  bool mNormalized;

  typedef llvm::StringMap<DataType> RSSpecificTypeMapTy;
  static llvm::ManagedStatic<RSSpecificTypeMapTy> RSSpecificTypeMap;

  static llvm::Type *RSObjectLLVMType;

  static const size_t SizeOfDataTypeInBits[];
  // @T was normalized by calling RSExportType::NormalizeType() before calling
  // this.
  // @TypeName was retrieved from RSExportType::GetTypeName() before calling
  // this
  static RSExportPrimitiveType *Create(RSContext *Context,
                                       const clang::Type *T,
                                       const llvm::StringRef &TypeName,
                                       DataKind DK = DataKindUser,
                                       bool Normalized = false);

 protected:
  RSExportPrimitiveType(RSContext *Context,
                        // for derived class to set their type class
                        ExportClass Class,
                        const llvm::StringRef &Name,
                        DataType DT,
                        DataKind DK,
                        bool Normalized)
      : RSExportType(Context, Class, Name),
        mType(DT),
        mKind(DK),
        mNormalized(Normalized) {
    return;
  }

  virtual const llvm::Type *convertToLLVMType() const;
  virtual union RSType *convertToSpecType() const;

  static DataType GetDataType(RSContext *Context, const clang::Type *T);

 public:
  // T is normalized by calling RSExportType::NormalizeType() before
  // calling this
  static bool IsPrimitiveType(const clang::Type *T);

  // @T may not be normalized
  static RSExportPrimitiveType *Create(RSContext *Context,
                                       const clang::Type *T,
                                       DataKind DK = DataKindUser);

  static DataType GetRSSpecificType(const llvm::StringRef &TypeName);
  static DataType GetRSSpecificType(const clang::Type *T);

  static bool IsRSMatrixType(DataType DT);
  static bool IsRSObjectType(DataType DT);
  static bool IsRSObjectType(const clang::Type *T) {
    return IsRSObjectType(GetRSSpecificType(T));
  }

  // Determines whether T is [an array of] struct that contains at least one
  // RS object type within it.
  static bool IsStructureTypeWithRSObject(const clang::Type *T);

  static size_t GetSizeInBits(const RSExportPrimitiveType *EPT);

  inline DataType getType() const { return mType; }
  inline DataKind getKind() const { return mKind; }
  inline bool isRSObjectType() const {
    return ((mType >= FirstRSObjectType) && (mType <= LastRSObjectType));
  }

  virtual bool equals(const RSExportable *E) const;
};  // RSExportPrimitiveType


class RSExportPointerType : public RSExportType {
  friend class RSExportType;
  friend class RSExportFunc;
 private:
  const RSExportType *mPointeeType;

  RSExportPointerType(RSContext *Context,
                      const llvm::StringRef &Name,
                      const RSExportType *PointeeType)
      : RSExportType(Context, ExportClassPointer, Name),
        mPointeeType(PointeeType) {
    return;
  }

  // @PT was normalized by calling RSExportType::NormalizeType() before calling
  // this.
  static RSExportPointerType *Create(RSContext *Context,
                                     const clang::PointerType *PT,
                                     const llvm::StringRef &TypeName);

  virtual const llvm::Type *convertToLLVMType() const;
  virtual union RSType *convertToSpecType() const;
 public:
  virtual bool keep();

  inline const RSExportType *getPointeeType() const { return mPointeeType; }

  virtual bool equals(const RSExportable *E) const;
};  // RSExportPointerType


class RSExportVectorType : public RSExportPrimitiveType {
  friend class RSExportType;
  friend class RSExportElement;
 private:
  unsigned mNumElement;   // number of element

  RSExportVectorType(RSContext *Context,
                     const llvm::StringRef &Name,
                     DataType DT,
                     DataKind DK,
                     bool Normalized,
                     unsigned NumElement)
      : RSExportPrimitiveType(Context, ExportClassVector, Name,
                              DT, DK, Normalized),
        mNumElement(NumElement) {
    return;
  }

  // @EVT was normalized by calling RSExportType::NormalizeType() before
  // calling this.
  static RSExportVectorType *Create(RSContext *Context,
                                    const clang::ExtVectorType *EVT,
                                    const llvm::StringRef &TypeName,
                                    DataKind DK = DataKindUser,
                                    bool Normalized = false);

  virtual const llvm::Type *convertToLLVMType() const;
  virtual union RSType *convertToSpecType() const;
 public:
  static llvm::StringRef GetTypeName(const clang::ExtVectorType *EVT);

  inline unsigned getNumElement() const { return mNumElement; }

  virtual bool equals(const RSExportable *E) const;
};

// Only *square* *float* matrix is supported by now.
//
// struct rs_matrix{2x2,3x3,4x4, ..., NxN} should be defined as the following
// form *exactly*:
//  typedef struct {
//    float m[{NxN}];
//  } rs_matrixNxN;
//
//  where mDim will be N.
class RSExportMatrixType : public RSExportType {
  friend class RSExportType;
 private:
  unsigned mDim;  // dimension

  RSExportMatrixType(RSContext *Context,
                     const llvm::StringRef &Name,
                     unsigned Dim)
    : RSExportType(Context, ExportClassMatrix, Name),
      mDim(Dim) {
    return;
  }

  virtual const llvm::Type *convertToLLVMType() const;
  virtual union RSType *convertToSpecType() const;
 public:
  // @RT was normalized by calling RSExportType::NormalizeType() before
  // calling this.
  static RSExportMatrixType *Create(RSContext *Context,
                                    const clang::RecordType *RT,
                                    const llvm::StringRef &TypeName,
                                    unsigned Dim);

  inline unsigned getDim() const { return mDim; }

  virtual bool equals(const RSExportable *E) const;
};

class RSExportConstantArrayType : public RSExportType {
  friend class RSExportType;
 private:
  const RSExportType *mElementType;  // Array element type
  unsigned mSize;  // Array size

  RSExportConstantArrayType(RSContext *Context,
                            const RSExportType *ElementType,
                            unsigned Size)
    : RSExportType(Context,
                   ExportClassConstantArray,
                   DUMMY_TYPE_NAME_FOR_RS_CONSTANT_ARRAY_TYPE),
      mElementType(ElementType),
      mSize(Size) {
    return;
  }

  // @CAT was normalized by calling RSExportType::NormalizeType() before
  // calling this.
  static RSExportConstantArrayType *Create(RSContext *Context,
                                           const clang::ConstantArrayType *CAT);

  virtual const llvm::Type *convertToLLVMType() const;
  virtual union RSType *convertToSpecType() const;
 public:
  inline unsigned getSize() const { return mSize; }
  inline const RSExportType *getElementType() const { return mElementType; }

  virtual bool keep();
  virtual bool equals(const RSExportable *E) const;
};

class RSExportRecordType : public RSExportType {
  friend class RSExportType;
 public:
  class Field {
   private:
    const RSExportType *mType;
    // Field name
    std::string mName;
    // Link to the struct that contain this field
    const RSExportRecordType *mParent;
    // Offset in the container
    size_t mOffset;

   public:
    Field(const RSExportType *T,
          const llvm::StringRef &Name,
          const RSExportRecordType *Parent,
          size_t Offset)
        : mType(T),
          mName(Name.data(), Name.size()),
          mParent(Parent),
          mOffset(Offset) {
      return;
    }

    inline const RSExportRecordType *getParent() const { return mParent; }
    inline const RSExportType *getType() const { return mType; }
    inline const std::string &getName() const { return mName; }
    inline size_t getOffsetInParent() const { return mOffset; }
  };

  typedef std::list<const Field*>::const_iterator const_field_iterator;

  inline const_field_iterator fields_begin() const {
    return this->mFields.begin();
  }
  inline const_field_iterator fields_end() const {
    return this->mFields.end();
  }

 private:
  std::list<const Field*> mFields;
  bool mIsPacked;
  // Artificial export struct type is not exported by user (and thus it won't
  // get reflected)
  bool mIsArtificial;
  size_t mAllocSize;

  RSExportRecordType(RSContext *Context,
                     const llvm::StringRef &Name,
                     bool IsPacked,
                     bool IsArtificial,
                     size_t AllocSize)
      : RSExportType(Context, ExportClassRecord, Name),
        mIsPacked(IsPacked),
        mIsArtificial(IsArtificial),
        mAllocSize(AllocSize) {
    return;
  }

  // @RT was normalized by calling RSExportType::NormalizeType() before calling
  // this.
  // @TypeName was retrieved from RSExportType::GetTypeName() before calling
  // this.
  static RSExportRecordType *Create(RSContext *Context,
                                    const clang::RecordType *RT,
                                    const llvm::StringRef &TypeName,
                                    bool mIsArtificial = false);

  virtual const llvm::Type *convertToLLVMType() const;
  virtual union RSType *convertToSpecType() const;
 public:
  inline const std::list<const Field*>& getFields() const { return mFields; }
  inline bool isPacked() const { return mIsPacked; }
  inline bool isArtificial() const { return mIsArtificial; }
  inline size_t getAllocSize() const { return mAllocSize; }

  virtual bool keep();
  virtual bool equals(const RSExportable *E) const;

  ~RSExportRecordType() {
    for (std::list<const Field*>::iterator I = mFields.begin(),
             E = mFields.end();
         I != E;
         I++)
      if (*I != NULL)
        delete *I;
    return;
  }
};  // RSExportRecordType

}   // namespace slang

#endif  // _FRAMEWORKS_COMPILE_SLANG_SLANG_RS_EXPORT_TYPE_H_  NOLINT
