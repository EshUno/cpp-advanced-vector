#pragma once
#include <cassert>
#include <cstdlib>
#include <new>
#include <utility>
#include <memory>

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

    RawMemory(RawMemory&& other) noexcept : buffer_(std::move(other.buffer_)), capacity_(other.capacity_)
    {
        other.buffer_ = nullptr;
        other.capacity_ = 0;
    }

    RawMemory& operator=(RawMemory&& other) noexcept {
        if (this != &other){
            buffer_ = std::move(other.buffer_);
            capacity_ = other.capacity_;
            other.buffer_ = nullptr;
            other.capacity_ = 0;
        }
        return *this;
    }

    ~RawMemory() {
        Deallocate(buffer_);
    }

    T* operator+(size_t offset) noexcept {
        // Разрешается получать адрес ячейки памяти, следующей за последним элементом массива
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

    [[nodiscard]] size_t Capacity() const {
        return capacity_;
    }

private:
    // Выделяет сырую память под n элементов и возвращает указатель на неё
    static T* Allocate(size_t n) {
        return n != 0 ? static_cast<T*>(operator new(n * sizeof(T))) : nullptr;
    }

    // Освобождает сырую память, выделенную ранее по адресу buf при помощи Allocate
    static void Deallocate(T* buf) noexcept {
        operator delete(buf);
    }

    T* buffer_ = nullptr;
    size_t capacity_ = 0;
};


template <typename T>
class Vector {
public:
    Vector() = default;

    explicit Vector(size_t size)
        : data_(size)
        , size_(size)
    {
        std::uninitialized_value_construct_n(begin(), size);
    }

    Vector(const Vector& other)
        : data_(other.size_)
        , size_(other.size_)  //
    {
        std::uninitialized_copy_n(other.begin(), other.size_, begin());
    }

    Vector(Vector&& other) noexcept : data_(std::move(other.data_)), size_(other.size_)
    {
        other.size_ = 0;
    }

    ~Vector() {
        std::destroy_n(begin(), size_);
    }

    using iterator = T*;
    using const_iterator = const T*;

    iterator begin() noexcept{
        return data_.GetAddress();
    }

    iterator end() noexcept{
        return data_.GetAddress() + size_;
    }

    const_iterator begin() const noexcept{
        return data_.GetAddress();
    }

    const_iterator end() const noexcept{
        return data_.GetAddress() + size_;
    }

    const_iterator cbegin() const noexcept{
        return data_.GetAddress();
    }

    const_iterator cend() const noexcept{
        return data_.GetAddress() + size_;
    }

    Vector& operator=(const Vector& rhs) {
        if (this != &rhs) {
            if (rhs.size_ > data_.Capacity()) {
                Vector rhs_copy(rhs);
                Swap(rhs_copy);
            } else {
                //  скопировать имеющиеся элементы из источника в приёмник, а избыточные элементы вектора-приёмника разрушить
                if (rhs.size_ < size_){
                    std::copy(rhs.begin(), rhs.begin() + rhs.size_, begin());
                    std::destroy_n(begin() + rhs.size_, size_ - rhs.size_);
                }
                // присвоить существующим элементам приёмника значения соответствующих элементов источника, а оставшиеся скопировать в свободную область
                else{
                    std::copy(rhs.begin(), rhs.begin() + size_, begin());
                    std::uninitialized_copy_n(rhs.begin() + size_, rhs.size_ - size_, begin() + size_);
                }
                size_ = rhs.size_;
            }
        }
        return *this;
    }

    Vector& operator=(Vector&& rhs) noexcept {
        if (this != &rhs){
            data_ = std::move(rhs.data_);
            size_ = rhs.size_;
            rhs.size_ = 0;
        }
        return *this;
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<Vector&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < size_);
        return data_[index];
    }

    void Swap(Vector& other) noexcept {
        data_.Swap(other.data_);
        std::swap(other.size_, size_);
    }

    [[nodiscard]] size_t Size() const noexcept {
        return size_;
    }

    [[nodiscard]] size_t Capacity() const noexcept {
        return data_.Capacity();
    }

    void Resize(size_t new_size){
        if (new_size < size_){
            std::destroy_n(begin() + new_size, size_ - new_size);
        }
        if (new_size > size_){
            Reserve(new_size);
            std::uninitialized_value_construct_n(begin(), new_size - size_);
        }
        size_ = new_size;
    }

    void PushBack(const T& value){
        if (size_ == Capacity()) {
            RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
            new(new_data.GetAddress() + size_) T(value);
            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                std::uninitialized_move_n(begin(), size_, new_data.GetAddress());
            } else {
                std::uninitialized_copy_n(begin(), size_, new_data.GetAddress());
            }
            std::destroy_n(begin(), size_);
            data_.Swap(new_data);
        }
        else{
            new (begin() + size_) T(value);
        }
        ++size_;
    }

    void PushBack(T&& value){
        if (size_ == Capacity()) {
            RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
            new(new_data.GetAddress() + size_) T(std::move(value));
            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                std::uninitialized_move_n(begin(), size_, new_data.GetAddress());
            } else {
                std::uninitialized_copy_n(begin(), size_, new_data.GetAddress());
            }
            std::destroy_n(begin(), size_);
            data_.Swap(new_data);
        }
        else{
            new (begin() + size_) T(std::move(value));
        }
        ++size_;
    }

    void PopBack() noexcept {
        if (size_ > 0){
            std::destroy_n(end() - 1, 1);
            --size_;
        }
    }

    template <typename... Args>
    T& EmplaceBack(Args&&... args) {
        if (size_ == Capacity()){
            RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
            new(new_data.GetAddress() + size_) T(std::forward<Args>(args)...);
            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                std::uninitialized_move_n(begin(), size_, new_data.GetAddress());
            } else {
                std::uninitialized_copy_n(begin(), size_, new_data.GetAddress());
            }

            std::destroy_n(begin(), size_);
            data_.Swap(new_data);
        } else {
            new (begin() + size_) T(std::forward<Args>(args)...);
        }
        ++size_;
        return *(end() - 1);
    }

    template <typename... Args>
    iterator Emplace(const_iterator pos, Args&&... args){
        size_t ind = std::distance(cbegin(), pos);
        if (size_ == Capacity()){

            RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
            new(new_data.GetAddress() + ind) T(std::forward<Args>(args)...);

            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                std::uninitialized_move_n(begin(), ind , new_data.GetAddress());
                std::uninitialized_move_n(begin() + ind, size_ - ind , new_data.GetAddress() + ind + 1);
            } else {
                std::uninitialized_copy_n(begin(), ind , new_data.GetAddress());
                std::uninitialized_copy_n(begin() + ind, size_ - ind , new_data.GetAddress() + ind + 1);
            }

            std::destroy_n(begin(), size_);
            data_.Swap(new_data);
        }else{
            if (pos == end()){
                new (begin() + size_) T(std::forward<Args>(args)...);
            }
            else {

               T tmp(std::forward<Args>(args)...);
               new (begin() + size_) T(std::forward<T>(*(end() - 1)));
               std::move_backward(begin() + ind, end() - 1, end());
               data_[ind] = std::move(tmp);

            }
        }
        ++size_;
        return begin() + ind;
    }

    iterator Insert(const_iterator pos, const T& value){
        return Emplace(pos, value);
    }

    iterator Insert(const_iterator pos, T&& value){
        return Emplace(pos, std::move(value));
    }

    iterator Erase(const_iterator pos){
        assert(size_ > 0);
        size_t ind = std::distance(cbegin(), pos);
        std::move(begin() + ind + 1, end(), begin() + ind);
        std::destroy_n(end() - 1, 1);
        --size_;
        return begin() + ind;
    }

    void Reserve(size_t new_capacity) {
        if (new_capacity <= data_.Capacity()) {
            return;
        }
        RawMemory<T> new_data(new_capacity);
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(begin(), size_, new_data.GetAddress());
        } else {
            std::uninitialized_copy_n(begin(), size_, new_data.GetAddress());
        }
        std::destroy_n(begin(), size_);
        data_.Swap(new_data);
    }

private:
    RawMemory<T> data_;
    size_t size_ = 0;
};
