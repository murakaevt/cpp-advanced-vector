#pragma once
#include <cassert>
#include <cstdlib>
#include <new>
#include <utility>
#include <memory>
#include <algorithm>

template <typename T>
class RawMemory {
public:
    RawMemory() = default;

    explicit RawMemory(size_t capacity)
        : buffer_(Allocate(capacity))
        , capacity_(capacity) {
    }

    RawMemory(const RawMemory&) = delete;
    RawMemory& operator=(const RawMemory& rhs) = delete;

    RawMemory(RawMemory&& other) noexcept {
        buffer_ = std::move(other.GetAddress());       
        capacity_ = std::move(other.Capacity());
    }

    RawMemory& operator=(RawMemory&& rhs) noexcept {
        buffer_ = std::move(rhs.GetAddress());
        capacity_ = std::move(rhs.Capacity());
        return *this;
    }

    ~RawMemory() {
        Deallocate(buffer_);
    }

    T* operator+(size_t offset) noexcept {
        // ����������� �������� ����� ������ ������, ��������� �� ��������� ��������� �������
        assert(offset <= capacity_);
        return buffer_ + offset;
    }

    const T* operator+(size_t offset) const noexcept {
        return const_cast<RawMemory&>(*this) + offset;
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<RawMemory&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < capacity_);
        return buffer_[index];
    }

    void Swap(RawMemory& other) noexcept {
        std::swap(buffer_, other.buffer_);
        std::swap(capacity_, other.capacity_);
    }

    const T* GetAddress() const noexcept {
        return buffer_;
    }

    T* GetAddress() noexcept {
        return buffer_;
    }

    size_t Capacity() const {
        return capacity_;
    }

private:
    // �������� ����� ������ ��� n ��������� � ���������� ��������� �� ��
    static T* Allocate(size_t n) {
        return n != 0 ? static_cast<T*>(operator new(n * sizeof(T))) : nullptr;
    }

    // ����������� ����� ������, ���������� ����� �� ������ buf ��� ������ Allocate
    static void Deallocate(T* buf) noexcept {
        operator delete(buf);
    }

    T* buffer_ = nullptr;
    size_t capacity_ = 0;
};

template <typename T>
class Vector {
public:
    using iterator = T*;
    using const_iterator = const T*;

    Vector() = default;

    //�����������, ��������� ������ ��������� �������. 
    explicit Vector(size_t size) 
    : data_(size),      
      size_(size) {       
        std::uninitialized_value_construct_n(data_.GetAddress(), size);
    }  

    // ����������� �����������
    Vector(const Vector& other)
    : data_(other.Size()),       
        size_(other.Size()) {
        std::uninitialized_copy_n(other.data_.GetAddress(), other.Size(), data_.GetAddress());        
    }

    // ����������� �����������
    Vector(Vector&& other) noexcept {
        Swap(other);
    }

    iterator begin() noexcept {
        return data_.GetAddress();
    }
    iterator end() noexcept {
        return data_.GetAddress() + size_;
    }
    const_iterator begin() const noexcept {
        return const_iterator(data_.GetAddress());
    }
    const_iterator end() const noexcept {
        return const_iterator(data_.GetAddress() + size_);
    }
    const_iterator cbegin() const noexcept {
        return begin();
    }
    const_iterator cend() const noexcept {
        return end();
    }

    Vector& operator=(const Vector& rhs) {
        if (this != &rhs) {
            if (rhs.size_ > data_.Capacity()) {
                Vector rhs_copy = Vector(rhs);
                Swap(rhs_copy);
            }
            else {   
                std::copy(rhs.data_.GetAddress(), rhs.data_.GetAddress() + std::min(rhs.size_,size_), data_.GetAddress());
                if (rhs.size_ < size_) {                   
                    std::destroy_n(data_.GetAddress() + rhs.size_, size_ - rhs.size_);
                }
                else {                   
                    std::uninitialized_copy_n(rhs.data_.GetAddress() + size_, rhs.size_ - size_, data_.GetAddress() + size_);
                }
                size_ = rhs.size_;
            }
        }       
        return *this;
    }

    Vector& operator=(Vector&& rhs) noexcept {
        if (this != &rhs) {
            Swap(rhs);
        }
        return *this;
    }

    void Swap(Vector& other) noexcept {
        data_.Swap(other.data_);
        std::swap(size_, other.size_);
    }

    size_t Size() const noexcept {
        return size_;
    }   

    void Resize(size_t new_size) {
        if (new_size < size_) {
            std::destroy_n(data_.GetAddress() + new_size, size_ - new_size);
            size_ = new_size;
        }
        else {
            Reserve(new_size);
            std::uninitialized_value_construct_n(data_.GetAddress() + size_, new_size - size_);
            size_ = new_size;
        }
    }

    template <typename Type>
    void PushBack(Type&& value) {
        EmplaceBack(std::forward<Type>(value));
    }

    void PopBack() noexcept {
        if (size_ > 0) {
            std::destroy_at(data_.GetAddress() + size_ - 1);
            --size_;
        }
    }

    template <typename... Args>
    T& EmplaceBack(Args&&... args) {
        return *Emplace(begin() + size_, std::forward<Args>(args)...);
    }

    template <typename... Args>
    iterator Emplace(const_iterator pos, Args&&... args) { 
        assert(pos >= begin() && pos <= end());
        size_t shift = pos - begin();
        iterator result = nullptr;
        if (size_ == Capacity()) {
            InsertionWithRelocation(shift, result, std::forward<Args>(args)...);
        }
        else {
            InsertionWithoutRelocation(shift, result, std::forward<Args>(args)...);
        }
            ++size_;        
        return result;
    }

    iterator Erase(const_iterator pos) noexcept(std::is_nothrow_move_assignable_v<T>) { 
        assert(pos >= begin() && pos < end());
        size_t shift = pos - begin();
        std::move(begin() + shift + 1, end(), begin() + shift);
        PopBack();
        return begin() + shift;
    }

    iterator Insert(const_iterator pos, const T& value) {
        return Emplace(pos, value);
    }

    iterator Insert(const_iterator pos, T&& value) {
        return Emplace(pos, std::move(value));
    }

    //� ����������� ��������� [] ������������ ��������  const_cast, 
    //����� ����� ������������� � ������ �� ������� ������ � ������� ������������� ������ ��������� [].
    //��� ��������� ���������� �� ������������ �������� assert(index < size).
    const T& operator[](size_t index) const noexcept {
        return const_cast<Vector&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < size_);
        return data_[index];
    }

    size_t Capacity() const noexcept {
        return data_.Capacity();
    }

    // ����� Reserve ������������ ��� ����������������� �������������� ������ ��� �������� �������, ����� �������� �� ��������� ����������
    void Reserve(size_t new_capacity) {
        if (new_capacity <= data_.Capacity()) {
            return;
        }
        RawMemory<T> new_data(new_capacity);
        ConstexprData(new_data);
    }

    ~Vector() {
        std::destroy_n(data_.GetAddress(), size_);
    }

private:
    RawMemory<T> data_;   
    size_t size_ = 0;   

    template <typename... Args>
    void InsertionWithRelocation(const size_t& shift, iterator& it, Args&&... args) {
        RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
        it = new (new_data + shift) T(std::forward<Args>(args)...);
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(data_.GetAddress(), shift, new_data.GetAddress());
            std::uninitialized_move_n(data_.GetAddress() + shift, size_ - shift, new_data.GetAddress() + shift + 1);
        }
        else {
            try {
                std::uninitialized_copy_n(data_.GetAddress(), shift, new_data.GetAddress());
                std::uninitialized_copy_n(data_.GetAddress() + shift, size_ - shift, new_data.GetAddress() + shift + 1);
            }
            catch (...) {
                std::destroy_n(new_data.GetAddress() + shift, 1);
                throw;
            }
        }
        std::destroy_n(begin(), size_);
        data_.Swap(new_data);
    }

    template <typename... Args>
    void InsertionWithoutRelocation(const size_t& shift, iterator& it, Args&&... args) {
        if (size_ != 0) {
            new (data_ + size_) T(std::move(*(data_.GetAddress() + size_ - 1)));
            try {
                std::move_backward(begin() + shift,
                    data_.GetAddress() + size_,
                    data_.GetAddress() + size_ + 1);
            }
            catch (...) {
                std::destroy_n(data_.GetAddress() + size_, 1);
                throw;
            }
            std::destroy_at(begin() + shift);
        }
        it = new (data_ + shift) T(std::forward<Args>(args)...);
    } 

    void ConstexprData(RawMemory<T>& new_data) {
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
        }
        else {
            std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
        }
        std::destroy_n(data_.GetAddress(), size_);
        data_.Swap(new_data);
    }
};